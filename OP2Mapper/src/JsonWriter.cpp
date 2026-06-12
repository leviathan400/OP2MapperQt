#include "JsonWriter.h"

#include <algorithm>

namespace JsonWriter {

QString escape(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar ch : s) {
        switch (ch.unicode()) {
        case '"':  out += QStringLiteral("\\\""); break;
        case '\\': out += QStringLiteral("\\\\"); break;
        case '\b': out += QStringLiteral("\\b");  break;
        case '\f': out += QStringLiteral("\\f");  break;
        case '\n': out += QStringLiteral("\\n");  break;
        case '\r': out += QStringLiteral("\\r");  break;
        case '\t': out += QStringLiteral("\\t");  break;
        default:
            if (ch.unicode() < 0x20) {
                out += QStringLiteral("\\u%1")
                           .arg(static_cast<int>(ch.unicode()), 4, 16,
                                QLatin1Char('0'));
            } else {
                out += ch;
            }
        }
    }
    return out;
}

QString paddedRow(const QVector<int> &values, int minPad)
{
    // Width = widest number in this row, floored at minPad — per-row, so a
    // row of small indices stays compact while a row containing e.g. 2034
    // aligns all its entries to 4 chars.
    int width = minPad;
    for (int v : values)
        width = std::max(width, static_cast<int>(QString::number(v).size()));

    QStringList padded;
    padded.reserve(values.size());
    for (int v : values)
        padded.append(QString::number(v).rightJustified(width, QLatin1Char(' ')));

    return QStringLiteral("[ ") + padded.join(QStringLiteral(", "))
         + QStringLiteral(" ]");
}

QString joinCrlf(const QStringList &lines)
{
    return lines.join(QStringLiteral("\r\n"));
}

} // namespace JsonWriter
