#pragma once

#include <QString>

// OP2Mapper.log next to the exe (same file and line format as OP2Mapper3):
//   yyyy-MM-dd HH:mm:ss.fff  [Category] message
// Rolled over to OP2Mapper.old.log once it passes 1 MB. Thread-safe and
// never throws: an unwritable folder just means nothing is logged.
namespace AppLog {

void write(const QString &category, const QString &message);
void writeStartup(const QStringList &arguments);

} // namespace AppLog
