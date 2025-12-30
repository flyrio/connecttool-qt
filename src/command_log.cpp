#include "command_log.h"

#include <QMetaObject>
#include <QThread>
#include <QtDebug>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <streambuf>

namespace {
QtMessageHandler g_prevHandler = nullptr;

class LogStreamBuf : public std::streambuf {
public:
  explicit LogStreamBuf(std::streambuf *fallback) : fallback_(fallback) {}

protected:
  int overflow(int ch) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ch == traits_type::eof()) {
      return traits_type::not_eof(ch);
    }
    const char value = static_cast<char>(ch);
    buffer_.push_back(value);
    if (fallback_) {
      fallback_->sputc(value);
    }
    if (value == '\n') {
      flushLine();
    }
    return ch;
  }

  std::streamsize xsputn(const char *s, std::streamsize n) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fallback_) {
      fallback_->sputn(s, n);
    }
    buffer_.append(s, static_cast<size_t>(n));
    flushLine();
    return n;
  }

  int sync() override {
    std::lock_guard<std::mutex> lock(mutex_);
    flushLine(true);
    if (fallback_) {
      return fallback_->pubsync();
    }
    return 0;
  }

private:
  void flushLine(bool force = false) {
    for (;;) {
      const auto pos = buffer_.find('\n');
      if (pos == std::string::npos) {
        break;
      }
      const std::string line = buffer_.substr(0, pos);
      buffer_.erase(0, pos + 1);
      if (!line.empty() || force) {
        CommandLog::instance().append(QString::fromLocal8Bit(line.c_str()));
      }
    }
    if (force && !buffer_.empty()) {
      const std::string line = buffer_;
      buffer_.clear();
      CommandLog::instance().append(QString::fromLocal8Bit(line.c_str()));
    }
  }

  std::string buffer_;
  std::streambuf *fallback_;
  std::mutex mutex_;
};

void messageHandler(QtMsgType type, const QMessageLogContext &context,
                    const QString &msg) {
  Q_UNUSED(context);
  QString prefix;
  switch (type) {
  case QtDebugMsg:
    prefix = QStringLiteral("[debug] ");
    break;
  case QtInfoMsg:
    prefix = QStringLiteral("[info] ");
    break;
  case QtWarningMsg:
    prefix = QStringLiteral("[warn] ");
    break;
  case QtCriticalMsg:
    prefix = QStringLiteral("[error] ");
    break;
  case QtFatalMsg:
    prefix = QStringLiteral("[fatal] ");
    break;
  }
  CommandLog::instance().append(prefix + msg);
  if (g_prevHandler) {
    g_prevHandler(type, context, msg);
  }
}
} // namespace

CommandLog &CommandLog::instance() {
  static CommandLog instance;
  return instance;
}

void CommandLog::install() {
  static bool installed = false;
  if (installed) {
    return;
  }
  installed = true;
  g_prevHandler = qInstallMessageHandler(messageHandler);

  static LogStreamBuf coutBuf(std::cout.rdbuf());
  static LogStreamBuf cerrBuf(std::cerr.rdbuf());
  std::cout.rdbuf(&coutBuf);
  std::cerr.rdbuf(&cerrBuf);
}

CommandLog::CommandLog(QObject *parent) : QObject(parent) {}

QString CommandLog::text() const { return text_; }

void CommandLog::append(const QString &text) {
  if (text.isEmpty()) {
    return;
  }
  if (QThread::currentThread() == thread()) {
    appendInternal(text);
    return;
  }
  const QString copy = text;
  QMetaObject::invokeMethod(this, [this, copy]() { appendInternal(copy); },
                            Qt::QueuedConnection);
}

void CommandLog::appendInternal(const QString &text) {
  QString normalized = text;
  normalized.replace("\r\n", "\n");
  normalized.replace('\r', '\n');
  QStringList incoming = normalized.split('\n', Qt::KeepEmptyParts);
  if (!incoming.isEmpty() && incoming.last().isEmpty()) {
    incoming.removeLast();
  }
  for (const auto &line : incoming) {
    lines_.append(line);
  }
  if (lines_.size() > maxLines_) {
    const int extra = lines_.size() - maxLines_;
    lines_.erase(lines_.begin(), lines_.begin() + extra);
  }
  text_ = lines_.join('\n');
  emit textChanged();
}
