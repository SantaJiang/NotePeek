#include "editor/documentformat.h"

#include <limits>

#include <QtEndian>
#include <QVector>

namespace {

bool isValidUtf8(const QByteArray &bytes)
{
    const auto *data = reinterpret_cast<const unsigned char *>(bytes.constData());
    const qsizetype size = bytes.size();
    qsizetype index = 0;
    while (index < size)
    {
        const unsigned char first = data[index++];
        if (first <= 0x7f)
        {
            continue;
        }

        int trailing = 0;
        quint32 codePoint = 0;
        quint32 minimum = 0;
        if ((first & 0xe0) == 0xc0)
        {
            trailing = 1;
            codePoint = first & 0x1f;
            minimum = 0x80;
        }
        else if ((first & 0xf0) == 0xe0)
        {
            trailing = 2;
            codePoint = first & 0x0f;
            minimum = 0x800;
        }
        else if ((first & 0xf8) == 0xf0)
        {
            trailing = 3;
            codePoint = first & 0x07;
            minimum = 0x10000;
        }
        else
        {
            return false;
        }

        if (index + trailing > size)
        {
            return false;
        }
        for (int i = 0; i < trailing; ++i)
        {
            const unsigned char next = data[index++];
            if ((next & 0xc0) != 0x80)
            {
                return false;
            }
            codePoint = (codePoint << 6) | (next & 0x3f);
        }
        if (codePoint < minimum || codePoint > 0x10ffff
            || (codePoint >= 0xd800 && codePoint <= 0xdfff))
        {
            return false;
        }
    }
    return true;
}

bool decodeUtf16(const QByteArray &bytes, bool littleEndian, QString *text)
{
    QVector<ushort> units;
    units.reserve(bytes.size() / 2);
    const auto *data = reinterpret_cast<const uchar *>(bytes.constData());
    for (qsizetype index = 0; index + 1 < bytes.size(); index += 2)
    {
        units.append(littleEndian ? qFromLittleEndian<quint16>(data + index)
                                  : qFromBigEndian<quint16>(data + index));
    }

    for (qsizetype index = 0; index < units.size(); ++index)
    {
        const ushort unit = units.at(index);
        if (unit >= 0xd800 && unit <= 0xdbff)
        {
            if (++index >= units.size())
                return false;
            const ushort trailing = units.at(index);
            if (trailing < 0xdc00 || trailing > 0xdfff)
                return false;
        }
        else if (unit >= 0xdc00 && unit <= 0xdfff)
        {
            return false;
        }
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    *text = QString::fromUtf16(reinterpret_cast<const char16_t *>(units.constData()), units.size());
#else
    *text = QString::fromUtf16(units.constData(), units.size());
#endif
    return true;
}

DocumentLineEnding detectLineEnding(const QString &text, DocumentLineEnding fallback)
{
    qsizetype crlf = 0;
    qsizetype lf = 0;
    qsizetype cr = 0;
    for (qsizetype index = 0; index < text.size(); ++index)
    {
        if (text.at(index) == QLatin1Char('\r'))
        {
            if (index + 1 < text.size() && text.at(index + 1) == QLatin1Char('\n'))
            {
                ++crlf;
                ++index;
            }
            else
            {
                ++cr;
            }
        }
        else if (text.at(index) == QLatin1Char('\n'))
        {
            ++lf;
        }
    }

    const int kinds = int(crlf > 0) + int(lf > 0) + int(cr > 0);
    if (kinds == 0)
        return fallback;
    if (kinds > 1)
        return DocumentLineEnding::Mixed;
    if (crlf > 0)
        return DocumentLineEnding::CrLf;
    if (cr > 0)
        return DocumentLineEnding::Cr;
    return DocumentLineEnding::Lf;
}

QString withLineEnding(const QString &text, DocumentLineEnding lineEnding)
{
    const QString normalized = DocumentFormatCodec::normalizedEditorText(text);
    if (lineEnding == DocumentLineEnding::CrLf)
        return QString(normalized).replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
    if (lineEnding == DocumentLineEnding::Cr)
        return QString(normalized).replace(QLatin1Char('\n'), QLatin1Char('\r'));
    return normalized;
}

} // namespace

DocumentFormat DocumentFormatCodec::defaultFormat()
{
    DocumentFormat format;
#ifdef Q_OS_WIN
    format.lineEnding = DocumentLineEnding::CrLf;
#endif
    return format;
}

bool DocumentFormatCodec::decode(const QByteArray &bytes,
                                 QString *text,
                                 DocumentFormat *format,
                                 QString *errorMessage)
{
    DocumentFormat detected = defaultFormat();
    QString decoded;
    if (bytes.startsWith("\xef\xbb\xbf"))
    {
        const QByteArray payload = bytes.mid(3);
        if (!isValidUtf8(payload))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The file contains invalid UTF-8 data.");
            return false;
        }
        detected.encoding = DocumentEncoding::Utf8Bom;
        decoded = QString::fromUtf8(payload);
    }
    else if (bytes.startsWith("\xff\xfe"))
    {
        if ((bytes.size() - 2) % 2 != 0)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The UTF-16 file has an incomplete code unit.");
            return false;
        }
        detected.encoding = DocumentEncoding::Utf16LittleEndian;
        if (!decodeUtf16(bytes.mid(2), true, &decoded))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The UTF-16 file contains an invalid surrogate pair.");
            return false;
        }
    }
    else if (bytes.startsWith("\xfe\xff"))
    {
        if ((bytes.size() - 2) % 2 != 0)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The UTF-16 file has an incomplete code unit.");
            return false;
        }
        detected.encoding = DocumentEncoding::Utf16BigEndian;
        if (!decodeUtf16(bytes.mid(2), false, &decoded))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The UTF-16 file contains an invalid surrogate pair.");
            return false;
        }
    }
    else
    {
        if (!isValidUtf8(bytes))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The file is not valid UTF-8 or BOM-marked UTF-16 text.");
            return false;
        }
        detected.encoding = DocumentEncoding::Utf8;
        decoded = QString::fromUtf8(bytes);
    }

    detected.lineEnding = detectLineEnding(decoded, detected.lineEnding);
    if (text)
        *text = normalizedEditorText(decoded);
    if (format)
        *format = detected;
    return true;
}

bool DocumentFormatCodec::decodeWithEncoding(const QByteArray &bytes,
                                             DocumentEncoding encoding,
                                             QString *text,
                                             DocumentFormat *format,
                                             QString *errorMessage)
{
    DocumentFormat resolved = defaultFormat();
    resolved.encoding = encoding;
    QString decoded;

    if (encoding == DocumentEncoding::Utf8 || encoding == DocumentEncoding::Utf8Bom)
    {
        QByteArray payload = bytes;
        if (payload.startsWith("\xef\xbb\xbf"))
        {
            payload = payload.mid(3);
            resolved.encoding = DocumentEncoding::Utf8Bom;
        }
        else if (encoding == DocumentEncoding::Utf8Bom)
        {
            resolved.encoding = DocumentEncoding::Utf8;
        }
        if (!isValidUtf8(payload))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The file contains invalid UTF-8 data.");
            return false;
        }
        decoded = QString::fromUtf8(payload);
    }
    else
    {
        const bool littleEndian = encoding == DocumentEncoding::Utf16LittleEndian;
        QByteArray payload = bytes;
        if ((littleEndian && payload.startsWith("\xff\xfe"))
            || (!littleEndian && payload.startsWith("\xfe\xff")))
        {
            payload = payload.mid(2);
        }
        if (payload.size() % 2 != 0)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The UTF-16 file has an incomplete code unit.");
            return false;
        }
        if (!decodeUtf16(payload, littleEndian, &decoded))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The UTF-16 file contains an invalid surrogate pair.");
            return false;
        }
    }

    resolved.lineEnding = detectLineEnding(decoded, resolved.lineEnding);
    if (text)
        *text = normalizedEditorText(decoded);
    if (format)
        *format = resolved;
    return true;
}

QByteArray DocumentFormatCodec::encode(const QString &text, const DocumentFormat &format)
{
    const DocumentLineEnding outputEnding = format.lineEnding == DocumentLineEnding::Mixed
                                                ? DocumentLineEnding::Lf
                                                : format.lineEnding;
    const QString output = withLineEnding(text, outputEnding);
    if (format.encoding == DocumentEncoding::Utf8)
        return output.toUtf8();
    if (format.encoding == DocumentEncoding::Utf8Bom)
        return QByteArray("\xef\xbb\xbf", 3) + output.toUtf8();

    const bool littleEndian = format.encoding == DocumentEncoding::Utf16LittleEndian;
    const qsizetype unitCount = output.size();
    QByteArray bytes;
    bytes.resize(2 + unitCount * 2);
    uchar *out = reinterpret_cast<uchar *>(bytes.data());
    out[0] = littleEndian ? 0xff : 0xfe;
    out[1] = littleEndian ? 0xfe : 0xff;
    out += 2;
    const ushort *units = reinterpret_cast<const ushort *>(output.utf16());
    if (littleEndian)
    {
        for (qsizetype index = 0; index < unitCount; ++index)
            qToLittleEndian<quint16>(units[index], out + index * 2);
    }
    else
    {
        for (qsizetype index = 0; index < unitCount; ++index)
            qToBigEndian<quint16>(units[index], out + index * 2);
    }
    return bytes;
}

QString DocumentFormatCodec::normalizedEditorText(const QString &text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return normalized;
}
