#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <vector>

class ChatModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
  enum Roles {
    SteamIdRole = Qt::UserRole + 1,
    DisplayNameRole,
    AvatarRole,
    MessageRole,
    IsSelfRole,
    TimestampRole
  };

  struct Entry {
    QString steamId;
    QString displayName;
    QString avatar;
    QString message;
    bool isSelf = false;
    QDateTime timestamp;
  };

  explicit ChatModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void appendMessage(Entry entry);
  void clear();

  int count() const { return static_cast<int>(entries_.size()); }

signals:
  void countChanged();

private:
  std::vector<Entry> entries_;
  int maxMessages_ = 200;
};
