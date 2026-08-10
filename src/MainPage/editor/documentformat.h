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
    Latin1 = 8
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

bool isLegacyEncoding(DocumentEncoding encoding);
// The encodings implemented with Qt 6 APIs and offered in the UI.
QList<DocumentEncoding> supportedEncodings();
// Short label used in menus and the status bar ("UTF-8", "UTF-16 LE", ...).
QString encodingName(DocumentEncoding encoding);
// False when re-encoding text into the target would replace characters it
// cannot represent, which would silently lose them on save.
bool canRepresent(const QString &text, DocumentEncoding encoding);

DocumentFormat defaultFormat();
bool decode(const QByteArray &bytes, QString *text, DocumentFormat *format, QString *errorMessage);
// Decodes directly to the UTF-8/LF representation consumed by Scintilla. The
// common UTF-8/LF path shares the input QByteArray instead of round-tripping
// through a UTF-16 QString.
bool decodeToEditorBytes(const QByteArray &bytes,
                         QByteArray *utf8Content,
                         DocumentFormat *format,
                         QString *errorMessage);
// Reinterprets the bytes in the given encoding instead of auto-detecting.
// A matching BOM is consumed; Utf8 and Utf8Bom interpret identically (the
// recorded encoding follows the BOM actually present).
bool decodeWithEncoding(const QByteArray &bytes,
                        DocumentEncoding encoding,
                        QString *text,
                        DocumentFormat *format,
                        QString *errorMessage);
bool decodeToEditorBytesWithEncoding(const QByteArray &bytes,
                                     DocumentEncoding encoding,
                                     QByteArray *utf8Content,
                                     DocumentFormat *format,
                                     QString *errorMessage);
QByteArray encode(const QString &text, const DocumentFormat &format);
QString normalizedEditorText(const QString &text);

} // namespace DocumentFormatCodec

#endif // DOCUMENTFORMAT_H
