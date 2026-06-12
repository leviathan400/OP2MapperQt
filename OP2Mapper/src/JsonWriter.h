#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Low-level helpers for hand-rolled JSON output.
//
// Why not QJsonDocument: QJsonObject stores keys SORTED, so its serialized
// key order can never match the VB.net reference implementations
// (OP2Mapper3 / OP2MapJsonTools), whose output the OP2 community treats as
// the canonical on-disk format. Tile-group and full-map JSON written by
// this app must be byte-identical to what the VB tools produce for the
// same data — including key order, 2-space indent, CRLF newlines, no BOM,
// no trailing newline, and the PerRowPadded single-line number rows. The
// only way to guarantee that is to emit text directly.
namespace JsonWriter {

// Escapes a string body for embedding in a JSON string literal (no quotes
// added). Matches Newtonsoft.Json's default behavior: the two mandatory
// escapes (backslash, quote), shorthand escapes for common control chars,
// \u00XX for the rest, and non-ASCII passed through raw (UTF-8 output).
QString escape(const QString &s);

// One PerRowPadded number row: `[ nnnn, nnnn, nnnn ]` with each number
// right-justified to the widest entry in THIS row (per-row max, not global),
// floored at minPad. This replicates VB PadNumbersInRow, which both
// OP2Mapper3 (tile groups, minPad 4) and OP2MapJsonTools (tiles 4 /
// cellTypes 2) use. The surrounding indent and trailing comma are the
// caller's job.
QString paddedRow(const QVector<int> &values, int minPad);

// Joins lines with CRLF — VB builds output with Environment.NewLine, which
// is CRLF on Windows, so the reference files are CRLF regardless of what
// platform we run on. No trailing newline (String.Join semantics).
QString joinCrlf(const QStringList &lines);

} // namespace JsonWriter
