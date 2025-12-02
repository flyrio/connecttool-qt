#include "multiplex_manager.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>

namespace {
constexpr std::size_t kTunnelChunkBytes = 60 * 1024;

// Simple, local ID generator to avoid pulling in the full nanoid dependency
std::string generateId(std::size_t length = 6) {
  static constexpr char chars[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<std::size_t> dist(0, sizeof(chars) - 2);

  std::string id;
  id.reserve(length);
  for (std::size_t i = 0; i < length; ++i) {
    id.push_back(chars[dist(rng)]);
  }
  return id;
}
} // namespace

MultiplexManager::MultiplexManager(ISteamNetworkingSockets *steamInterface,
                                   HSteamNetConnection steamConn,
                                   boost::asio::io_context &io_context,
                                   bool &isHost, int &localPort)
    : steamInterface_(steamInterface), steamConn_(steamConn),
      io_context_(io_context), isHost_(isHost), localPort_(localPort) {
  sendTimer_ = std::make_unique<boost::asio::steady_timer>(io_context_);
}

MultiplexManager::~MultiplexManager() {
  // Close all sockets
  std::lock_guard<std::mutex> lock(mapMutex_);
  for (auto &pair : clientMap_) {
    pair.second->close();
  }
  clientMap_.clear();
}

std::string MultiplexManager::addClient(std::shared_ptr<tcp::socket> socket) {
  std::string id;
  {
    std::lock_guard<std::mutex> lock(mapMutex_);
    do {
      id = generateId(6);
    } while (clientMap_.find(id) != clientMap_.end());

    clientMap_[id] = socket;
    readBuffers_[id].resize(1048576);
    missingClients_.erase(id);
  }
  startAsyncRead(id);
  std::cout << "Added client with id " << id << std::endl;
  return id;
}

bool MultiplexManager::removeClient(const std::string &id) {
  bool removed = false;
  std::lock_guard<std::mutex> lock(mapMutex_);
  auto it = clientMap_.find(id);
  if (it != clientMap_.end()) {
    it->second->close();
    clientMap_.erase(it);
    removed = true;
  }
  readBuffers_.erase(id);
  missingClients_.erase(id);

  if (removed) {
    std::cout << "Removed client with id " << id << std::endl;
  }
  {
    std::lock_guard<std::mutex> queueLock(queueMutex_);
    pendingPackets_.erase(id);
  }
  return removed;
}

std::shared_ptr<tcp::socket>
MultiplexManager::getClient(const std::string &id) {
  std::lock_guard<std::mutex> lock(mapMutex_);
  auto it = clientMap_.find(id);
  if (it != clientMap_.end()) {
    return it->second;
  }
  return nullptr;
}

std::vector<char> MultiplexManager::buildPacket(const std::string &id,
                                                const char *data, size_t len,
                                                int type) const {
  const size_t idLen = id.size() + 1;
  const size_t payloadLen = (type == 0 ? len : 0);
  const size_t packetSize = idLen + sizeof(uint32_t) + payloadLen;
  std::vector<char> packet(packetSize);
  std::memcpy(packet.data(), id.c_str(), idLen);
  auto *pType = reinterpret_cast<uint32_t *>(packet.data() + idLen);
  *pType = type;
  if (payloadLen > 0 && data) {
    std::memcpy(packet.data() + idLen + sizeof(uint32_t), data, payloadLen);
  }
  return packet;
}

bool MultiplexManager::trySendPacket(const std::vector<char> &packet) {
  if (packet.empty()) {
    return true;
  }
  EResult result = steamInterface_->SendMessageToConnection(
      steamConn_, packet.data(), static_cast<uint32>(packet.size()),
      k_nSteamNetworkingSend_Reliable, nullptr);
  if (result == k_EResultOK) {
    return true;
  }
  if (result == k_EResultLimitExceeded) {
    return false;
  }

  if (result != k_EResultNoConnection && result != k_EResultInvalidParam) {
    std::cerr << "[Multiplex] SendMessageToConnection failed with result "
              << static_cast<int>(result) << std::endl;
  }
  return true;
}

void MultiplexManager::enqueuePacket(const std::string &id,
                                     std::vector<char> packet) {
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    pendingPackets_[id].push_back(std::move(packet));
  }
  scheduleFlush();
}

void MultiplexManager::flushPendingPackets() {
  std::unique_lock<std::mutex> lock(queueMutex_);
  for (auto it = pendingPackets_.begin(); it != pendingPackets_.end();) {
    auto &queue = it->second;
    while (!queue.empty()) {
      const std::vector<char> &packet = queue.front();
      lock.unlock();
      const bool sent = trySendPacket(packet);
      lock.lock();
      if (!sent) {
        return;
      }
      queue.pop_front();
    }
    if (queue.empty()) {
      it = pendingPackets_.erase(it);
    } else {
      ++it;
    }
  }
}

void MultiplexManager::scheduleFlush() {
  bool needSchedule = false;
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (!flushScheduled_) {
      flushScheduled_ = true;
      needSchedule = true;
    }
  }
  if (!needSchedule) {
    return;
  }

  sendTimer_->expires_after(std::chrono::milliseconds(5));
  sendTimer_->async_wait([this](const boost::system::error_code &ec) {
    if (!ec) {
      flushPendingPackets();
    }
    bool shouldReschedule = false;
    {
      std::lock_guard<std::mutex> lock(queueMutex_);
      flushScheduled_ = false;
      shouldReschedule = !pendingPackets_.empty();
    }
    if (shouldReschedule) {
      scheduleFlush();
    }
  });
}

void MultiplexManager::sendTunnelPacket(const std::string &id, const char *data,
                                        size_t len, int type) {
  auto pushPacket = [this, &id](const char *ptr, size_t amount,
                                int packetType) {
    auto packet = buildPacket(id, ptr, amount, packetType);
    if (!trySendPacket(packet)) {
      enqueuePacket(id, std::move(packet));
    }
  };

  if (type == 0 && data && len > kTunnelChunkBytes) {
    size_t offset = 0;
    while (offset < len) {
      const size_t chunk = std::min(kTunnelChunkBytes, len - offset);
      pushPacket(data + offset, chunk, 0);
      offset += chunk;
    }
    return;
  }

  pushPacket(data, len, type);
}

void MultiplexManager::handleTunnelPacket(const char *data, size_t len) {
  size_t idLen = 7; // 6 + null
  if (len < idLen + sizeof(uint32_t)) {
    std::cerr << "Invalid tunnel packet size" << std::endl;
    return;
  }
  std::string id(data, 6);
  uint32_t type = *reinterpret_cast<const uint32_t *>(data + idLen);
  if (type == 0) {
    // Data packet
    size_t dataLen = len - idLen - sizeof(uint32_t);
    const char *packetData = data + idLen + sizeof(uint32_t);
    auto socket = getClient(id);
    if (!socket && isHost_ && localPort_ > 0) {
      // 如果是主持且没有对应的 TCP Client，创建一个连接到本地端口
      std::cout << "Creating new TCP client for id " << id
                << " connecting to localhost:" << localPort_ << std::endl;
      try {
        auto newSocket = std::make_shared<tcp::socket>(io_context_);
        tcp::resolver resolver(io_context_);
        auto endpoints =
            resolver.resolve("127.0.0.1", std::to_string(localPort_));
        boost::asio::connect(*newSocket, endpoints);

        std::string tempId = id;
        {
          std::lock_guard<std::mutex> lock(mapMutex_);
          clientMap_[id] = newSocket;
          readBuffers_[id].resize(1048576);
          socket = newSocket;
        }
        std::cout << "Successfully created TCP client for id " << id
                  << std::endl;
        startAsyncRead(tempId);
      } catch (const std::exception &e) {
        std::cerr << "Failed to create TCP client for id " << id << ": "
                  << e.what() << std::endl;
        sendTunnelPacket(id, nullptr, 0, 1);
        return;
      }
    }
    if (socket) {
      missingClients_.erase(id);
      auto payload =
          std::make_shared<std::vector<char>>(packetData, packetData + dataLen);
      boost::asio::async_write(
          *socket, boost::asio::buffer(*payload),
          [this, id, payload](const boost::system::error_code &writeEc,
                              std::size_t) {
            if (writeEc) {
              std::cout << "Error writing to TCP client " << id << ": "
                        << writeEc.message() << std::endl;
              removeClient(id);
            }
          });
    } else {
      if (missingClients_.insert(id).second) {
        std::cerr << "No client found for id " << id << std::endl;
      }
      sendTunnelPacket(id, nullptr, 0, 1);
    }
  } else if (type == 1) {
    // Disconnect packet
    if (removeClient(id)) {
      std::cout << "Client " << id << " disconnected" << std::endl;
    }
  } else {
    std::cerr << "Unknown packet type " << type << std::endl;
  }
}

void MultiplexManager::startAsyncRead(const std::string &id) {
  auto socket = getClient(id);
  if (!socket) {
    std::cout << "Error: Socket is null for id " << id << std::endl;
    return;
  }
  socket->async_read_some(
      boost::asio::buffer(readBuffers_[id]),
      [this, id](const boost::system::error_code &ec,
                 std::size_t bytes_transferred) {
        if (!ec) {
          if (bytes_transferred > 0) {
            sendTunnelPacket(id, readBuffers_[id].data(), bytes_transferred, 0);
          }
          startAsyncRead(id);
        } else {
          std::cout << "Error reading from TCP client " << id << ": "
                    << ec.message() << std::endl;
          removeClient(id);
        }
      });
}
