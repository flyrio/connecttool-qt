#include "chat_model.h"

ChatModel::ChatModel(QObject *parent) : QAbstractListModel(parent) {}

int ChatModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(entries_.size());
}

QVariant ChatModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return {};
  }
  const auto row = static_cast<size_t>(index.row());
  if (row >= entries_.size()) {
    return {};
  }
  const auto &entry = entries_[row];
  switch (role) {
  case SteamIdRole:
    return entry.steamId;
  case DisplayNameRole:
    return entry.displayName;
  case AvatarRole:
    return entry.avatar;
  case MessageRole:
    return entry.message;
  case IsSelfRole:
    return entry.isSelf;
  case TimestampRole:
    return entry.timestamp;
  default:
    return {};
  }
}

QHash<int, QByteArray> ChatModel::roleNames() const {
  return {{SteamIdRole, "steamId"},       {DisplayNameRole, "displayName"},
          {AvatarRole, "avatar"},         {MessageRole, "message"},
          {IsSelfRole, "isSelf"},         {TimestampRole, "timestamp"}};
}

void ChatModel::appendMessage(Entry entry) {
  const int insertRow = static_cast<int>(entries_.size());
  beginInsertRows(QModelIndex(), insertRow, insertRow);
  entries_.push_back(std::move(entry));
  endInsertRows();
  emit countChanged();

  const int overflow = static_cast<int>(entries_.size()) - maxMessages_;
  if (overflow > 0) {
    beginRemoveRows(QModelIndex(), 0, overflow - 1);
    entries_.erase(entries_.begin(), entries_.begin() + overflow);
    endRemoveRows();
    emit countChanged();
  }
}

void ChatModel::clear() {
  if (entries_.empty()) {
    return;
  }
  beginResetModel();
  entries_.clear();
  endResetModel();
  emit countChanged();
}
