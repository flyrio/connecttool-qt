#pragma once

#include <QAbstractListModel>
#include <QString>
#include <vector>

class LobbiesModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
  enum Roles {
    LobbyIdRole = Qt::UserRole + 1,
    NameRole,
    HostNameRole,
    HostIdRole,
    MemberCountRole,
    PingRole,
  };

  struct Entry {
    QString lobbyId;
    QString name;
    QString hostName;
    QString hostId;
    int memberCount = 0;
    int ping = -1;
  };

  explicit LobbiesModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setLobbies(std::vector<Entry> list);
  int count() const { return static_cast<int>(entries_.size()); }

signals:
  void countChanged();

private:
  std::vector<Entry> entries_;
};
