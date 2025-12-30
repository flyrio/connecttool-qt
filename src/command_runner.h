#pragma once

#include <QString>
#include <QStringList>

namespace command {

struct Result {
  int exitCode = -1;
  QString output;
};

Result runHidden(const QString &program, const QStringList &arguments);
void logIfNeeded(const QString &program, const QStringList &arguments,
                 const Result &result);

} // namespace command
