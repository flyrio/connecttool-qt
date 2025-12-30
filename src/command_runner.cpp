#include "command_runner.h"

#include "command_log.h"

#include <QProcess>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace command {
namespace {
QString quoteArg(const QString &arg) {
  if (arg.isEmpty()) {
    return QStringLiteral("\"\"");
  }
  if (!arg.contains(' ') && !arg.contains('"')) {
    return arg;
  }
  QString escaped = arg;
  escaped.replace('"', "\\\"");
  return QStringLiteral("\"%1\"").arg(escaped);
}

QString formatCommand(const QString &program, const QStringList &arguments) {
  QStringList parts;
  parts.reserve(arguments.size() + 1);
  parts.append(program);
  for (const auto &arg : arguments) {
    parts.append(quoteArg(arg));
  }
  return parts.join(' ');
}
} // namespace

Result runHidden(const QString &program, const QStringList &arguments) {
  QProcess process;
  process.setProgram(program);
  process.setArguments(arguments);
  process.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
  process.setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
      });
#endif
  process.start();
  if (!process.waitForStarted()) {
    return {-1, QStringLiteral("Failed to start: %1").arg(process.errorString())};
  }
  process.waitForFinished(-1);
  QString output = QString::fromLocal8Bit(process.readAll());
  while (output.endsWith('\n') || output.endsWith('\r')) {
    output.chop(1);
  }
  const int exitCode =
      process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
  return {exitCode, output};
}

void logIfNeeded(const QString &program, const QStringList &arguments,
                 const Result &result) {
  if (result.exitCode == 0 && result.output.isEmpty()) {
    return;
  }
  QString message = QStringLiteral("[cmd] %1")
                        .arg(formatCommand(program, arguments));
  if (!result.output.isEmpty()) {
    message.append('\n');
    message.append(result.output);
  }
  if (result.exitCode != 0) {
    message.append('\n');
    message.append(QStringLiteral("[cmd] exit code %1").arg(result.exitCode));
  }
  CommandLog::instance().append(message);
}

} // namespace command
