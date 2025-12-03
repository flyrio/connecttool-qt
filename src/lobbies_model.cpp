#include "lobbies_model.h"

LobbiesModel::LobbiesModel(QObject *parent) : QAbstractListModel(parent) {}

int LobbiesModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(entries_.size());
}

QVariant LobbiesModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= rowCount(index.parent())) {
    return {};
  }

  const auto &entry = entries_[static_cast<std::size_t>(index.row())];
  switch (role) {
  case LobbyIdRole:
    return entry.lobbyId;
  case NameRole:
    return entry.name;
  case HostNameRole:
    return entry.hostName;
  case HostIdRole:
    return entry.hostId;
  case MemberCountRole:
    return entry.memberCount;
  case PingRole:
    return entry.ping >= 0 ? QVariant(entry.ping) : QVariant();
  default:
    return {};
  }
}

QHash<int, QByteArray> LobbiesModel::roleNames() const {
  return {{LobbyIdRole, "lobbyId"},     {NameRole, "name"},
          {HostNameRole, "hostName"},   {HostIdRole, "hostId"},
          {MemberCountRole, "members"}, {PingRole, "ping"}};
}

void LobbiesModel::setLobbies(std::vector<Entry> list) {
  const bool sizeChanged = list.size() != entries_.size();
  if (sizeChanged) {
    beginResetModel();
    entries_ = std::move(list);
    endResetModel();
    emit countChanged();
    return;
  }

  entries_ = std::move(list);
  if (!entries_.empty()) {
    emit dataChanged(index(0, 0),
                     index(static_cast<int>(entries_.size()) - 1, 0));
  }
}
