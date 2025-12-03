#pragma once
#include <functional>
#include <iostream>
#include <mutex>
#include <steam_api.h>
#include <string>
#include <vector>

class SteamNetworkingManager; // Forward declaration
class SteamRoomManager;       // Forward declaration for callbacks

class SteamFriendsCallbacks {
public:
  SteamFriendsCallbacks(SteamNetworkingManager *manager,
                        SteamRoomManager *roomManager);

private:
  SteamNetworkingManager *manager_;
  SteamRoomManager *roomManager_;

  STEAM_CALLBACK(SteamFriendsCallbacks, OnGameLobbyJoinRequested,
                 GameLobbyJoinRequested_t);
};

class SteamMatchmakingCallbacks {
public:
  SteamMatchmakingCallbacks(SteamNetworkingManager *manager,
                            SteamRoomManager *roomManager);

  CCallResult<SteamMatchmakingCallbacks, LobbyCreated_t>
      m_CallResultLobbyCreated;
  CCallResult<SteamMatchmakingCallbacks, LobbyMatchList_t>
      m_CallResultLobbyMatchList;

  void OnLobbyCreated(LobbyCreated_t *pCallback, bool bIOFailure);
  void OnLobbyListReceived(LobbyMatchList_t *pCallback, bool bIOFailure);

private:
  SteamNetworkingManager *manager_;
  SteamRoomManager *roomManager_;

  STEAM_CALLBACK(SteamMatchmakingCallbacks, OnLobbyEntered, LobbyEnter_t);
  STEAM_CALLBACK(SteamMatchmakingCallbacks, OnLobbyChatUpdate,
                 LobbyChatUpdate_t);
};

class SteamRoomManager {
public:
  struct LobbyInfo {
    CSteamID id;
    CSteamID ownerId;
    std::string name;
    std::string ownerName;
    int memberCount = 0;
    int pingMs = -1;
  };

  SteamRoomManager(SteamNetworkingManager *networkingManager);
  ~SteamRoomManager();

  bool createLobby();
  void leaveLobby();
  bool searchLobbies();
  bool joinLobby(CSteamID lobbyID);
  bool startHosting();
  void stopHosting();
  void setLobbyName(const std::string &name);
  void setPublishLobby(bool publish);
  std::string getLobbyName() const;
  void setLobbyListCallback(
      std::function<void(const std::vector<LobbyInfo> &)> callback);

  CSteamID getCurrentLobby() const { return currentLobby; }
  const std::vector<CSteamID> &getLobbies() const { return lobbies; }
  const std::vector<LobbyInfo> &getLobbyInfos() const { return lobbyInfos; }
  std::vector<CSteamID> getLobbyMembers() const;

  void setCurrentLobby(CSteamID lobby) { currentLobby = lobby; }
  void addLobby(CSteamID lobby) { lobbies.push_back(lobby); }
  void clearLobbies() { lobbies.clear(); }

private:
  friend class SteamMatchmakingCallbacks;
  friend class SteamFriendsCallbacks;
  friend class Backend;
  void refreshLobbyMetadata();
  void notifyLobbyListUpdated();

  SteamNetworkingManager *networkingManager_;
  CSteamID currentLobby;
  std::vector<CSteamID> lobbies;
  std::vector<LobbyInfo> lobbyInfos;
  SteamFriendsCallbacks *steamFriendsCallbacks;
  SteamMatchmakingCallbacks *steamMatchmakingCallbacks;
  std::function<void(const std::vector<LobbyInfo> &)> lobbyListCallback_;
  std::string lobbyName_;
  bool publishLobby_ = true;
};
