#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class CommandLog : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString text READ text NOTIFY textChanged)

public:
  static CommandLog &instance();
  static void install();

  QString text() const;
  void append(const QString &text);

signals:
  void textChanged();

private:
  explicit CommandLog(QObject *parent = nullptr);
  void appendInternal(const QString &text);

  QStringList lines_;
  QString text_;
  int maxLines_ = 200;
};
