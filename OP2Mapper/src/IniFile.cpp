// IniFile: OP2Mapper3-compatible reader / writer for Mapper3.ini.

#include "IniFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTextStream>

namespace {

// The whole file as lines (CR stripped); empty if it can't be read.
QStringList readLines()
{
    QFile f(IniFile::path());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QString text = QString::fromUtf8(f.readAll());
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString &l : lines) {
        if (l.endsWith(QLatin1Char('\r')))
            l.chop(1);
    }
    // A trailing newline produces one empty element; drop it so rewrites
    // don't grow the file.
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    return lines;
}

bool isSection(const QString &line, QString *name = nullptr)
{
    const QString t = line.trimmed();
    if (t.size() >= 2 && t.startsWith(QLatin1Char('[')) && t.endsWith(QLatin1Char(']'))) {
        if (name)
            *name = t.mid(1, t.size() - 2).trimmed();
        return true;
    }
    return false;
}

// VB rule: the value is everything after the first "=", not trimmed and not unescaped.
bool keyMatches(const QString &line, const QString &key, QString *valueOut)
{
    const QString t = line.trimmed();
    const QString prefix = key + QLatin1Char('=');
    if (!t.startsWith(prefix, Qt::CaseInsensitive))
        return false;
    if (valueOut) {
        // VB takes everything after the first '=' of the raw line.
        const int eq = line.indexOf(QLatin1Char('='));
        *valueOut = eq >= 0 ? line.mid(eq + 1) : QString();
    }
    return true;
}

bool find(const QStringList &lines, const QString &section, const QString &key,
          QString *valueOut)
{
    bool inSection = false;
    for (const QString &line : lines) {
        QString name;
        if (isSection(line, &name)) {
            inSection = name.compare(section, Qt::CaseInsensitive) == 0;
            continue;
        }
        if (inSection && keyMatches(line, key, valueOut))
            return true;
    }
    return false;
}

} // namespace

namespace IniFile {

QString path()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Mapper3.ini"));
}

QString value(const QString &section, const QString &key, const QString &defaultValue)
{
    QString v;
    if (find(readLines(), section, key, &v))
        return v;
    return defaultValue;
}

bool contains(const QString &section, const QString &key)
{
    return find(readLines(), section, key, nullptr);
}

// Replace the key in place, or add it at the end of its section, or append
// the section. The file is rewritten with CRLF line ends.
void setValue(const QString &section, const QString &key, const QString &value)
{
    QStringList lines = readLines();
    const QString entry = key + QLatin1Char('=') + value;

    int sectionStart = -1;
    int sectionEnd = -1; // index after the section's last line
    bool done = false;
    for (int i = 0; i < lines.size(); ++i) {
        QString name;
        if (isSection(lines[i], &name)) {
            if (sectionStart >= 0 && sectionEnd < 0)
                sectionEnd = i;
            if (sectionStart < 0 && name.compare(section, Qt::CaseInsensitive) == 0)
                sectionStart = i;
            continue;
        }
        if (sectionStart >= 0 && sectionEnd < 0 && keyMatches(lines[i], key, nullptr)) {
            lines[i] = entry;
            done = true;
            break;
        }
    }
    if (!done) {
        if (sectionStart >= 0) {
            if (sectionEnd < 0)
                sectionEnd = lines.size();
            // Insert after the last non-blank line of the section.
            int insertAt = sectionEnd;
            while (insertAt - 1 > sectionStart && lines[insertAt - 1].trimmed().isEmpty())
                --insertAt;
            lines.insert(insertAt, entry);
        } else {
            if (!lines.isEmpty() && !lines.last().trimmed().isEmpty())
                lines.append(QString());
            lines.append(QLatin1Char('[') + section + QLatin1Char(']'));
            lines.append(entry);
        }
    }

    QFile f(path());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return; // ignored, like the VB writer
    f.write((lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n")).toUtf8());
}

// "True" / "False" in any case, or a number (non-zero is true).
bool boolValue(const QString &section, const QString &key, bool defaultValue)
{
    const QString v = value(section, key).trimmed();
    if (v.compare(QLatin1String("True"), Qt::CaseInsensitive) == 0)
        return true;
    if (v.compare(QLatin1String("False"), Qt::CaseInsensitive) == 0)
        return false;
    bool ok = false;
    const int n = v.toInt(&ok);
    return ok ? n != 0 : defaultValue;
}

void setBool(const QString &section, const QString &key, bool v)
{
    setValue(section, key, v ? QStringLiteral("True") : QStringLiteral("False"));
}

int intValue(const QString &section, const QString &key, int defaultValue)
{
    bool ok = false;
    const int n = value(section, key).trimmed().toInt(&ok);
    return ok ? n : defaultValue;
}

void setInt(const QString &section, const QString &key, int v)
{
    setValue(section, key, QString::number(v));
}

} // namespace IniFile
