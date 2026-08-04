#ifndef DOCUMENTFORMAT_H
#define DOCUMENTFORMAT_H

#include <QByteArray>
#include <QList>
#include <QString>

// Persisted by value in places that record a document's format, so existing
// entries must keep their numbers: append new encodings at the end.
enum class DocumentEncoding
{
    Utf8,
    Utf8Bom,
    Utf16LittleEndian,
    Utf16BigEndian,
    // Legacy (non-Unicode) code pages.  Source files written on Chinese,
    // Japanese and Korean systems still commonly use these for comments and
    // string literals, and they only round-trip byte-for-byte when the
    // document remembers which one it came from.
    Gb18030,
    Big5,
    ShiftJis,
    EucKr,
    Latin1
};

enum class DocumentLineEnding
{
    Lf,
    CrLf,
    Cr,
    Mixed
};

struct DocumentFormat
{
    DocumentEncoding encoding = DocumentEncoding::Utf8;
    DocumentLineEnding lineEnding = DocumentLineEnding::Lf;
};

namespace DocumentFormatCodec {

// True when this build can convert the legacy code pages.  Qt 5 always can;
// Qt 6 needs the Core5Compat module, which is optional at configure time.
bool legacyEncodingsAvailable();
bool isLegacyEncoding(DocumentEncoding encoding);
// The list offered in the UI: UTF variants always, legacy code pages only
// where legacyEncodingsAvailable().
QList<DocumentEncoding> supportedEncodings();
// Short label used in menus and the status bar ("UTF-8", "GB18030", ...).
QString encodingName(DocumentEncoding encoding);
// False when re-encoding text into the target would replace characters it
// cannot represent, which would silently lose them on save.
bool canRepresent(const QString &text, DocumentEncoding encoding);

DocumentFormat defaultFormat();
bool decode(const QByteArray &bytes, QString *text, DocumentFormat *format, QString *errorMessage);
// Reinterprets the bytes in the given encoding instead of auto-detecting.
// A matching BOM is consumed; Utf8 and Utf8Bom interpret identically (the
// recorded encoding follows the BOM actually present).
bool decodeWithEncoding(const QByteArray &bytes,
                        DocumentEncoding encoding,
                        QString *text,
                        DocumentFormat *format,
                        QString *errorMessage);
QByteArray encode(const QString &text, const DocumentFormat &format);
QString normalizedEditorText(const QString &text);

} // namespace DocumentFormatCodec

#endif // DOCUMENTFORMAT_H
