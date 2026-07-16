#ifndef DOCUMENTFORMAT_H
#define DOCUMENTFORMAT_H

#include <QByteArray>
#include <QString>

enum class DocumentEncoding
{
    Utf8,
    Utf8Bom,
    Utf16LittleEndian,
    Utf16BigEndian
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

DocumentFormat defaultFormat();
bool decode(const QByteArray &bytes, QString *text, DocumentFormat *format, QString *errorMessage);
QByteArray encode(const QString &text, const DocumentFormat &format);
QString normalizedEditorText(const QString &text);

} // namespace DocumentFormatCodec

#endif // DOCUMENTFORMAT_H
