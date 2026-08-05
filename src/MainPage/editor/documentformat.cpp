#include "editor/documentformat.h"

#include <limits>

#include <QtEndian>
#include <QVector>

// Qt 5 ships QTextCodec in QtCore.  Qt 6 moved it to the optional Core5Compat
// module, so the legacy code pages are only compiled in when the build found
// it (NOTEPEEK_HAVE_QT5COMPAT, set by CMake).
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) || defined(NOTEPEEK_HAVE_QT5COMPAT)
#define NOTEPEEK_HAVE_TEXT_CODECS 1
#include <QTextCodec>
#endif

namespace {

// Codec names as QTextCodec knows them.  Returns nullptr for the UTF
// encodings, which are decoded by hand above.
const char *legacyCodecName(DocumentEncoding encoding)
{
    switch (encoding)
    {
    case DocumentEncoding::Gb18030:
        return "GB18030";
    case DocumentEncoding::Big5:
        return "Big5";
    case DocumentEncoding::ShiftJis:
        return "Shift-JIS";
    case DocumentEncoding::EucKr:
        return "EUC-KR";
    case DocumentEncoding::Latin1:
        return "ISO-8859-1";
    case DocumentEncoding::Utf8:
    case DocumentEncoding::Utf8Bom:
    case DocumentEncoding::Utf16LittleEndian:
    case DocumentEncoding::Utf16BigEndian:
        break;
    }
    return nullptr;
}

#ifdef NOTEPEEK_HAVE_TEXT_CODECS
QTextCodec *legacyCodec(DocumentEncoding encoding)
{
    const char *name = legacyCodecName(encoding);
    return name ? QTextCodec::codecForName(name) : nullptr;
}
#endif

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

bool DocumentFormatCodec::legacyEncodingsAvailable()
{
#ifdef NOTEPEEK_HAVE_TEXT_CODECS
    return legacyCodec(DocumentEncoding::Gb18030) != nullptr;
#else
    return false;
#endif
}

bool DocumentFormatCodec::isLegacyEncoding(DocumentEncoding encoding)
{
    return legacyCodecName(encoding) != nullptr;
}

QList<DocumentEncoding> DocumentFormatCodec::supportedEncodings()
{
    QList<DocumentEncoding> encodings = {
        DocumentEncoding::Utf8,
        DocumentEncoding::Utf8Bom,
        DocumentEncoding::Utf16LittleEndian,
        DocumentEncoding::Utf16BigEndian,
    };
    if (legacyEncodingsAvailable())
    {
        encodings.append({
            DocumentEncoding::Gb18030,
            DocumentEncoding::Big5,
            DocumentEncoding::ShiftJis,
            DocumentEncoding::EucKr,
            DocumentEncoding::Latin1,
        });
    }
    return encodings;
}

QString DocumentFormatCodec::encodingName(DocumentEncoding encoding)
{
    switch (encoding)
    {
    case DocumentEncoding::Utf8:
        return QStringLiteral("UTF-8");
    case DocumentEncoding::Utf8Bom:
        return QStringLiteral("UTF-8 BOM");
    case DocumentEncoding::Utf16LittleEndian:
        return QStringLiteral("UTF-16 LE");
    case DocumentEncoding::Utf16BigEndian:
        return QStringLiteral("UTF-16 BE");
    case DocumentEncoding::Gb18030:
        return QStringLiteral("GB18030");
    case DocumentEncoding::Big5:
        return QStringLiteral("Big5");
    case DocumentEncoding::ShiftJis:
        return QStringLiteral("Shift-JIS");
    case DocumentEncoding::EucKr:
        return QStringLiteral("EUC-KR");
    case DocumentEncoding::Latin1:
        return QStringLiteral("ISO-8859-1");
    }
    return QStringLiteral("UTF-8");
}

bool DocumentFormatCodec::canRepresent(const QString &text, DocumentEncoding encoding)
{
    if (!isLegacyEncoding(encoding))
    {
        // The UTF encodings cover the whole of Unicode.
        return true;
    }
#ifdef NOTEPEEK_HAVE_TEXT_CODECS
    QTextCodec *codec = legacyCodec(encoding);
    if (!codec)
    {
        return false;
    }
    QTextCodec::ConverterState state;
    codec->fromUnicode(text.constData(), int(text.size()), &state);
    return state.invalidChars == 0;
#else
    Q_UNUSED(text);
    return false;
#endif
}

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
        decoded = QString::fromUtf8(payload.constData(), payload.size());
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
    else if (isValidUtf8(bytes))
    {
        detected.encoding = DocumentEncoding::Utf8;
        decoded = QString::fromUtf8(bytes.constData(), bytes.size());
    }
    else
    {
        // Not Unicode.  GB18030 is the only legacy code page guessed here: it
        // is the one that turns up in practice (Chinese comments in source
        // files), and it round-trips byte-for-byte, so saving cannot corrupt
        // the file.  Anything it rejects is still reported as undecodable;
        // Tools ▸ Encoding ▸ Reopen with Encoding handles the rest.
        //
        // The acceptance test is a byte-faithful round-trip (re-encode the
        // candidate and compare to the input) rather than
        // state.invalidChars: that field is reported inconsistently by Qt's
        // GB18030 codec across platforms (e.g. a lone 0xFF, which is not a
        // valid GB18030 lead byte, sometimes counts as 0 invalid chars), so it
        // would silently accept corrupted bytes as GB18030.  A genuine
        // GB18030 sequence is the unique inverse of its bytes, so this check
        // both accepts real legacy text and rejects anything the codec only
        // "decoded" by substitution.
        bool decodedAsLegacy = false;
#ifdef NOTEPEEK_HAVE_TEXT_CODECS
        if (QTextCodec *codec = legacyCodec(DocumentEncoding::Gb18030))
        {
            QTextCodec::ConverterState state;
            const QString candidate = codec->toUnicode(bytes.constData(), int(bytes.size()), &state);
            if (codec->fromUnicode(candidate) == bytes)
            {
                detected.encoding = DocumentEncoding::Gb18030;
                decoded = candidate;
                decodedAsLegacy = true;
            }
        }
#endif
        if (!decodedAsLegacy)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The file is not valid UTF-8 or BOM-marked UTF-16 text.");
            return false;
        }
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
        decoded = QString::fromUtf8(payload.constData(), payload.size());
    }
    else if (isLegacyEncoding(encoding))
    {
#ifdef NOTEPEEK_HAVE_TEXT_CODECS
        QTextCodec *codec = legacyCodec(encoding);
        if (!codec)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("This build cannot read %1 files.")
                                    .arg(encodingName(encoding));
            return false;
        }
        QTextCodec::ConverterState state;
        decoded = codec->toUnicode(bytes.constData(), int(bytes.size()), &state);
        if (state.invalidChars > 0 || state.remainingChars > 0)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The file is not valid %1 text.")
                                    .arg(encodingName(encoding));
            return false;
        }
#else
        if (errorMessage)
            *errorMessage = QStringLiteral("This build cannot read %1 files.")
                                .arg(encodingName(encoding));
        return false;
#endif
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

    if (isLegacyEncoding(format.encoding))
    {
#ifdef NOTEPEEK_HAVE_TEXT_CODECS
        // Characters the code page cannot express become substitutes here;
        // callers warn before converting (see canRepresent()).
        if (QTextCodec *codec = legacyCodec(format.encoding))
            return codec->fromUnicode(output.constData(), int(output.size()), nullptr);
#endif
        // Without codec support the document could only be written as mojibake,
        // so fall back to UTF-8 rather than corrupting it.
        return output.toUtf8();
    }

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
