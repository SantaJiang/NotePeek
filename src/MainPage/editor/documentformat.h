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
    Latin1 = 8,

    // Legacy single- and multi-byte code pages. Qt 6 core has no codec for
    // these, so they are decoded/encoded through the platform text codec
    // (iconv on Unix, MultiByteToWideChar on Windows); see legacytextcodec.h.
    Gb18030 = 9,
    Big5 = 10,
    ShiftJis = 11,
    EucJp = 12,
    EucKr = 13,
    Windows1250 = 14,
    Windows1251 = 15,
    Windows1252 = 16,
    Windows1253 = 17,
    Windows1254 = 18,
    Windows1255 = 19,
    Windows1256 = 20,
    Windows1257 = 21,
    Windows1258 = 22,
    Iso88592 = 23,
    Iso88595 = 24,
    Iso88597 = 25,
    Iso885915 = 26,
    Koi8R = 27,
    Koi8U = 28,
    Tis620 = 29
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
// The encodings offered in the UI (Unicode first, then legacy code pages).
QList<DocumentEncoding> supportedEncodings();
// Legacy CJK codecs tried, in order, when auto-detecting a file that is not
// valid UTF-8/UTF-16. Single-byte codecs are excluded: every byte sequence is
// valid in them, so auto-detection is impossible and would misread binary data.
QList<DocumentEncoding> autoDetectEncodings();
// Detects the encoding of `bytes` (BOM, valid UTF-8, or a legacy CJK codec)
// and returns the number of BOM bytes to skip. Never fails: undecidable input
// resolves to Utf8 so the caller can report the usual invalid-UTF-8 error.
// `allowTrailingPartial` tolerates a trailing incomplete UTF-8 sequence, as
// produced when detecting on a bounded prefix of a larger file.
qsizetype detectAutoEncoding(const QByteArray &bytes, DocumentEncoding *encoding,
                             bool allowTrailingPartial = false);
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
