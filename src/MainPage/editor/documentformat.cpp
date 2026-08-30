#include "editor/documentformat.h"
#include "editor/legacytextcodec.h"

#include <QtEndian>
#include <QVector>

namespace {

bool isValidUtf8(const char *rawData, qsizetype size)
{
    const auto *data = reinterpret_cast<const unsigned char *>(rawData);
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

bool isValidUtf8(const QByteArray &bytes)
{
    return isValidUtf8(bytes.constData(), bytes.size());
}

// Like isValidUtf8, but tolerates a single incomplete multi-byte sequence at
// the very end. Used when detecting on a fixed-size prefix of a larger file,
// where the prefix may cut a UTF-8 character in half.
bool isValidUtf8Prefix(const char *rawData, qsizetype size)
{
    const auto *data = reinterpret_cast<const unsigned char *>(rawData);
    qsizetype index = 0;
    while (index < size)
    {
        const unsigned char first = data[index++];
        if (first <= 0x7f)
        {
            continue;
        }

        int trailing = 0;
        if ((first & 0xe0) == 0xc0)
        {
            trailing = 1;
        }
        else if ((first & 0xf0) == 0xe0)
        {
            trailing = 2;
        }
        else if ((first & 0xf8) == 0xf0)
        {
            trailing = 3;
        }
        else
        {
            return false;
        }

        if (index + trailing > size)
        {
            // Truncated at the end of the prefix: accept only when every
            // present continuation byte is valid.
            while (index < size)
            {
                if ((data[index] & 0xc0) != 0x80)
                    return false;
                ++index;
            }
            return true;
        }

        for (int i = 0; i < trailing; ++i)
        {
            if ((data[index] & 0xc0) != 0x80)
                return false;
            ++index;
        }
    }
    return true;
}

bool isCjkCharacter(ushort unit)
{
    return (unit >= 0x4E00 && unit <= 0x9FFF)    // CJK unified ideographs
        || (unit >= 0x3400 && unit <= 0x4DBF)    // extension A
        || (unit >= 0xF900 && unit <= 0xFAFF)    // compatibility ideographs
        || (unit >= 0x3040 && unit <= 0x30FF)    // full-width hiragana + katakana
        || (unit >= 0xAC00 && unit <= 0xD7AF)    // hangul syllables
        || (unit >= 0x3000 && unit <= 0x303F)    // CJK punctuation
        || (unit >= 0xFF00 && unit <= 0xFF60)    // full-width punctuation/Latin/digits
        || (unit >= 0xFFE0 && unit <= 0xFFEF);   // full-width forms (excl. half-width kana FF61-FF9F)
}

int cjkPlausibilityScore(const QString &text)
{
    int score = 0;
    for (QChar character : text)
        if (isCjkCharacter(character.unicode()))
            ++score;
    return score;
}

// Best-matching legacy CJK encoding for bytes that are not valid UTF-8, or
// Utf8 when none is plausible enough. Scoring (rather than "first that
// decodes") keeps arbitrary binary or very short input from being silently
// reinterpreted as CJK text. When `allowTrailingPartial` is set the input may
// be a bounded prefix that cut a multi-byte character, so a trailing partial
// sequence is trimmed and retried.
DocumentEncoding detectLegacyEncoding(const QByteArray &bytes, bool allowTrailingPartial = false)
{
    // A NUL byte is a strong binary / UTF-16-without-BOM signal.
    if (bytes.contains('\0'))
        return DocumentEncoding::Utf8;

    DocumentEncoding best = DocumentEncoding::Utf8;
    int bestScore = 0;
    for (DocumentEncoding candidate : DocumentFormatCodec::autoDetectEncodings())
    {
        QString text;
        if (LegacyTextCodec::decode(bytes, candidate, &text, nullptr))
        {
            const int score = cjkPlausibilityScore(text);
            if (score > bestScore)
            {
                bestScore = score;
                best = candidate;
            }
            continue;
        }
        if (!allowTrailingPartial)
            continue;
        // The longest CJK sequence is four bytes (GB18030), so trimming up to
        // three trailing bytes retries every possible split point.
        for (int trim = 1; trim <= 3 && trim < bytes.size(); ++trim)
        {
            const QByteArray trimmed = bytes.left(bytes.size() - trim);
            if (LegacyTextCodec::decode(trimmed, candidate, &text, nullptr))
            {
                const int score = cjkPlausibilityScore(text);
                if (score > bestScore)
                {
                    bestScore = score;
                    best = candidate;
                }
                break;
            }
        }
    }
    return bestScore >= 2 ? best : DocumentEncoding::Utf8;
}

DocumentLineEnding detectUtf8LineEnding(const char *data, qsizetype size,
                                        DocumentLineEnding fallback)
{
    qsizetype crlf = 0;
    qsizetype lf = 0;
    qsizetype cr = 0;
    for (qsizetype index = 0; index < size; ++index)
    {
        if (data[index] == '\r')
        {
            if (index + 1 < size && data[index + 1] == '\n')
            {
                ++crlf;
                ++index;
            }
            else
            {
                ++cr;
            }
        }
        else if (data[index] == '\n')
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

QByteArray normalizedUtf8Bytes(const QByteArray &bytes, qsizetype offset,
                               DocumentLineEnding ending)
{
    if (ending == DocumentLineEnding::Lf && offset == 0)
        return bytes;
    if (ending == DocumentLineEnding::Lf)
        return bytes.mid(offset);

    QByteArray normalized;
    normalized.reserve(bytes.size() - offset);
    const char *data = bytes.constData();
    for (qsizetype index = offset; index < bytes.size(); ++index)
    {
        if (data[index] == '\r')
        {
            if (index + 1 < bytes.size() && data[index + 1] == '\n')
                ++index;
            normalized.append('\n');
        }
        else
        {
            normalized.append(data[index]);
        }
    }
    return normalized;
}

bool decodeUtf8ToEditorBytes(const QByteArray &bytes, qsizetype offset,
                             DocumentEncoding encoding, QByteArray *utf8Content,
                             DocumentFormat *format, QString *errorMessage)
{
    const qsizetype payloadSize = bytes.size() - offset;
    if (!isValidUtf8(bytes.constData() + offset, payloadSize))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("The file contains invalid UTF-8 data.");
        return false;
    }

    DocumentFormat detected = DocumentFormatCodec::defaultFormat();
    detected.encoding = encoding;
    detected.lineEnding = detectUtf8LineEnding(bytes.constData() + offset,
                                               payloadSize,
                                               detected.lineEnding);
    if (utf8Content)
        *utf8Content = normalizedUtf8Bytes(bytes, offset, detected.lineEnding);
    if (format)
        *format = detected;
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

    *text = QString::fromUtf16(reinterpret_cast<const char16_t *>(units.constData()), units.size());
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

bool DocumentFormatCodec::isLegacyEncoding(DocumentEncoding encoding)
{
    switch (encoding)
    {
    case DocumentEncoding::Utf8:
    case DocumentEncoding::Utf8Bom:
    case DocumentEncoding::Utf16LittleEndian:
    case DocumentEncoding::Utf16BigEndian:
        return false;
    default:
        return true;
    }
}

QList<DocumentEncoding> DocumentFormatCodec::supportedEncodings()
{
    QList<DocumentEncoding> encodings = {
        DocumentEncoding::Utf8,
        DocumentEncoding::Utf8Bom,
        DocumentEncoding::Utf16LittleEndian,
        DocumentEncoding::Utf16BigEndian,
        DocumentEncoding::Latin1,
        DocumentEncoding::Gb18030,
        DocumentEncoding::Big5,
        DocumentEncoding::ShiftJis,
        DocumentEncoding::EucJp,
        DocumentEncoding::EucKr,
        DocumentEncoding::Windows1250,
        DocumentEncoding::Windows1251,
        DocumentEncoding::Windows1252,
        DocumentEncoding::Windows1253,
        DocumentEncoding::Windows1254,
        DocumentEncoding::Windows1255,
        DocumentEncoding::Windows1256,
        DocumentEncoding::Windows1257,
        DocumentEncoding::Windows1258,
        DocumentEncoding::Iso88592,
        DocumentEncoding::Iso88595,
        DocumentEncoding::Iso88597,
        DocumentEncoding::Iso885915,
        DocumentEncoding::Koi8R,
        DocumentEncoding::Koi8U,
        DocumentEncoding::Tis620,
    };
    return encodings;
}

QList<DocumentEncoding> DocumentFormatCodec::autoDetectEncodings()
{
    return {
        DocumentEncoding::Gb18030,
        DocumentEncoding::Big5,
        DocumentEncoding::ShiftJis,
        DocumentEncoding::EucJp,
        DocumentEncoding::EucKr,
    };
}

qsizetype DocumentFormatCodec::detectAutoEncoding(const QByteArray &bytes,
                                                  DocumentEncoding *encoding,
                                                  bool allowTrailingPartial)
{
    if (bytes.startsWith("\xef\xbb\xbf"))
    {
        *encoding = DocumentEncoding::Utf8Bom;
        return 3;
    }
    if (bytes.startsWith("\xff\xfe"))
    {
        *encoding = DocumentEncoding::Utf16LittleEndian;
        return 2;
    }
    if (bytes.startsWith("\xfe\xff"))
    {
        *encoding = DocumentEncoding::Utf16BigEndian;
        return 2;
    }
    if (isValidUtf8(bytes) || (allowTrailingPartial && isValidUtf8Prefix(bytes.constData(), bytes.size())))
    {
        *encoding = DocumentEncoding::Utf8;
        return 0;
    }
    *encoding = detectLegacyEncoding(bytes, allowTrailingPartial);
    return 0;
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
    case DocumentEncoding::Latin1:
        return QStringLiteral("ISO-8859-1");
    case DocumentEncoding::Gb18030:
        return QStringLiteral("GB18030");
    case DocumentEncoding::Big5:
        return QStringLiteral("Big5");
    case DocumentEncoding::ShiftJis:
        return QStringLiteral("Shift-JIS");
    case DocumentEncoding::EucJp:
        return QStringLiteral("EUC-JP");
    case DocumentEncoding::EucKr:
        return QStringLiteral("EUC-KR");
    case DocumentEncoding::Windows1250:
        return QStringLiteral("Windows-1250");
    case DocumentEncoding::Windows1251:
        return QStringLiteral("Windows-1251");
    case DocumentEncoding::Windows1252:
        return QStringLiteral("Windows-1252");
    case DocumentEncoding::Windows1253:
        return QStringLiteral("Windows-1253");
    case DocumentEncoding::Windows1254:
        return QStringLiteral("Windows-1254");
    case DocumentEncoding::Windows1255:
        return QStringLiteral("Windows-1255");
    case DocumentEncoding::Windows1256:
        return QStringLiteral("Windows-1256");
    case DocumentEncoding::Windows1257:
        return QStringLiteral("Windows-1257");
    case DocumentEncoding::Windows1258:
        return QStringLiteral("Windows-1258");
    case DocumentEncoding::Iso88592:
        return QStringLiteral("ISO-8859-2");
    case DocumentEncoding::Iso88595:
        return QStringLiteral("ISO-8859-5");
    case DocumentEncoding::Iso88597:
        return QStringLiteral("ISO-8859-7");
    case DocumentEncoding::Iso885915:
        return QStringLiteral("ISO-8859-15");
    case DocumentEncoding::Koi8R:
        return QStringLiteral("KOI8-R");
    case DocumentEncoding::Koi8U:
        return QStringLiteral("KOI8-U");
    case DocumentEncoding::Tis620:
        return QStringLiteral("TIS-620");
    }
    return QStringLiteral("UTF-8");
}

bool DocumentFormatCodec::canRepresent(const QString &text, DocumentEncoding encoding)
{
    if (!isLegacyEncoding(encoding))
        return true;

    if (encoding == DocumentEncoding::Latin1)
    {
        for (QChar character : text)
            if (character.unicode() > 0xff)
                return false;
        return true;
    }

    return LegacyTextCodec::canRepresent(text, encoding);
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
        // Not BOM-marked and not valid UTF-8: try the legacy CJK codecs so
        // GBK/GB18030/Big5/... files open without a manual re-open. Single-byte
        // codecs are never auto-detected (every byte sequence is valid in them).
        const DocumentEncoding legacy = detectLegacyEncoding(bytes);
        if (legacy == DocumentEncoding::Utf8)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("The file is not valid UTF-8 or BOM-marked UTF-16 text.");
            return false;
        }
        detected.encoding = legacy;
        if (!LegacyTextCodec::decode(bytes, legacy, &decoded, errorMessage))
            return false;
    }

    detected.lineEnding = detectLineEnding(decoded, detected.lineEnding);
    if (text)
        *text = normalizedEditorText(decoded);
    if (format)
        *format = detected;
    return true;
}

bool DocumentFormatCodec::decodeToEditorBytes(const QByteArray &bytes,
                                              QByteArray *utf8Content,
                                              DocumentFormat *format,
                                              QString *errorMessage)
{
    if (bytes.startsWith("\xef\xbb\xbf"))
        return decodeUtf8ToEditorBytes(bytes, 3, DocumentEncoding::Utf8Bom,
                                       utf8Content, format, errorMessage);
    if (!bytes.startsWith("\xff\xfe") && !bytes.startsWith("\xfe\xff"))
    {
        if (decodeUtf8ToEditorBytes(bytes, 0, DocumentEncoding::Utf8,
                                    utf8Content, format, errorMessage))
            return true;

        // Invalid UTF-8: fall back to the legacy CJK codecs before failing.
        const DocumentEncoding legacy = detectLegacyEncoding(bytes);
        if (legacy != DocumentEncoding::Utf8
            && decodeToEditorBytesWithEncoding(bytes, legacy, utf8Content,
                                               format, errorMessage))
            return true;

        if (errorMessage)
            *errorMessage = QStringLiteral("The file contains invalid UTF-8 data.");
        return false;
    }

    QString text;
    if (!decode(bytes, &text, format, errorMessage))
        return false;
    if (utf8Content)
        *utf8Content = text.toUtf8();
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
        if (encoding == DocumentEncoding::Latin1)
            decoded = QString::fromLatin1(bytes);
        else if (!LegacyTextCodec::decode(bytes, encoding, &decoded, errorMessage))
            return false;
    }
    else if (encoding == DocumentEncoding::Utf16LittleEndian
             || encoding == DocumentEncoding::Utf16BigEndian)
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
    else
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("The requested text encoding is not supported.");
        return false;
    }

    resolved.lineEnding = detectLineEnding(decoded, resolved.lineEnding);
    if (text)
        *text = normalizedEditorText(decoded);
    if (format)
        *format = resolved;
    return true;
}

bool DocumentFormatCodec::decodeToEditorBytesWithEncoding(
    const QByteArray &bytes, DocumentEncoding encoding, QByteArray *utf8Content,
    DocumentFormat *format, QString *errorMessage)
{
    if (encoding == DocumentEncoding::Utf8 || encoding == DocumentEncoding::Utf8Bom)
    {
        const bool hasBom = bytes.startsWith("\xef\xbb\xbf");
        return decodeUtf8ToEditorBytes(bytes, hasBom ? 3 : 0,
                                       hasBom ? DocumentEncoding::Utf8Bom
                                              : DocumentEncoding::Utf8,
                                       utf8Content, format, errorMessage);
    }

    QString text;
    if (!decodeWithEncoding(bytes, encoding, &text, format, errorMessage))
        return false;
    if (utf8Content)
        *utf8Content = text.toUtf8();
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
        if (format.encoding == DocumentEncoding::Latin1)
            return output.toLatin1();
        return LegacyTextCodec::encode(output, format.encoding);
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
