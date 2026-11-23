#pragma once

#include <QString>

// Minimal INI reader/writer that follows OP2Mapper3's hand-rolled rules, so
// the Qt and VB builds can share one Mapper3.ini:
//   - section header = trimmed line "[name]", names match case-insensitively
//   - key line = trimmed line starting with "key=" (case-insensitive); the
//     value is everything after '=', no quoting or escaping (raw paths)
//   - a write re-reads the file, replaces the key in place, or inserts it at
//     the end of its section, or appends the section; then rewrites the file
//   - read failures return the default; write failures are ignored
namespace IniFile {

// <exe dir>/Mapper3.ini, or the file named by the OP2MAPPER_INI environment
// variable when it is set.
QString path();

QString value(const QString &section, const QString &key,
              const QString &defaultValue = QString());
void setValue(const QString &section, const QString &key, const QString &value);
bool contains(const QString &section, const QString &key);

// Typed helpers with VB parsing rules ("True"/"False" any case, or an int).
bool boolValue(const QString &section, const QString &key, bool defaultValue);
void setBool(const QString &section, const QString &key, bool v);
int intValue(const QString &section, const QString &key, int defaultValue);
void setInt(const QString &section, const QString &key, int v);

} // namespace IniFile
