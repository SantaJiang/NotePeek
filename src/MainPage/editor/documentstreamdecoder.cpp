#include "editor/documentstreamdecoder.h"

#include <QtEndian>

namespace {

const char InvalidUtf8Message[] = "The file contains invalid UTF-8 data.";
const char IncompleteUtf16Message[] = "The UTF-16 file has an incomplete code unit.";
const char InvalidSurrogateMessage[] = "The UTF-16 file contains an invalid surrogate pair.";

// Upper bound on how much input is buffered before the encoding is committed.
// Mirrors the whole-file auto-detection window; a prefix this large contains
// non-ASCII bytes for any realistic text file, so CJK detection is reliable
// while streaming memory stays bounded.
constexpr qsizetype DetectionWindowBytes = 64 * 1024;

qsizetype appendUtf8CodePoint(quint32 codePoint, QByteArray *output)
{
    if (codePoint < 0x80)
    {
        output->append(char(codePoint));
        return 1;
    }
    if (codePoint < 0x800)
    {
        output->append(char(0xc0 | (codePoint >> 6)));
        output->append(char(0x80 | (codePoint & 0x3f)));
        return 2;
    }
    if (codePoint < 0x10000)
    {
        output->append(char(0xe0 | (codePoint >> 12)));
        output->append(char(0x80 | ((codePoint >> 6) & 0x3f)));
        output->append(char(0x80 | (codePoint & 0x3f)));
        return 3;
    }
    output->append(char(0xf0 | (codePoint >> 18)));
    output->append(char(0x80 | ((codePoint >> 12) & 0x3f)));
    output->append(char(0x80 | ((codePoint >> 6) & 0x3f)));
    output->append(char(0x80 | (codePoint & 0x3f)));
    return 4;
}

} // namespace

DocumentStreamDecoder::DocumentStreamDecoder()
{
}

void DocumentStreamDecoder::setEncodingOverride(DocumentEncoding encoding)
{
    m_overrideSet = true;
    m_override = encoding;
}

void DocumentStreamDecoder::appendOutput(QByteArray *output, char byte)
{
    output->append(byte);
    m_metadata.outputUtf8Bytes += 1;
}

void DocumentStreamDecoder::appendOutput(QByteArray *output, const char *data, qsizetype size)
{
    output->append(data, size);
    m_metadata.outputUtf8Bytes += size;
}

void DocumentStreamDecoder::flushPendingCr(QByteArray *output)
{
    if (!m_pendingCr)
        return;
    m_pendingCr = false;
    ++m_cr;
    appendOutput(output, '\n');
}

void DocumentStreamDecoder::recordEncoding(DocumentEncoding encoding)
{
    m_metadata.format.encoding = encoding;
}

void DocumentStreamDecoder::emitCodePoint(quint32 codePoint, QByteArray *output)
{
    if (m_pendingCr)
    {
        m_pendingCr = false;
        if (codePoint == '\n')
        {
            ++m_crlf;
            appendOutput(output, '\n');
            return;
        }
        ++m_cr;
        appendOutput(output, '\n');
    }

    if (codePoint == '\r')
    {
        m_pendingCr = true;
        return;
    }
    if (codePoint == '\n')
    {
        ++m_lf;
        appendOutput(output, '\n');
        return;
    }

    m_metadata.outputUtf8Bytes += appendUtf8CodePoint(codePoint, output);
}

// Feeds a decoded UTF-16 string through emitCodePoint so line-ending
// normalization and statistics stay identical to the whole-file path.
void DocumentStreamDecoder::emitDecodedText(const QString &decoded, QByteArray *output)
{
    const QChar *units = decoded.constData();
    const qsizetype count = decoded.size();
    qsizetype index = 0;
    while (index < count)
    {
        const QChar current = units[index];
        if (current.isHighSurrogate() && index + 1 < count && units[index + 1].isLowSurrogate())
        {
            emitCodePoint(QChar::surrogateToUcs4(current, units[index + 1]), output);
            index += 2;
        }
        else if (current.isSurrogate())
        {
            emitCodePoint(0xfffd, output); // lone surrogate: replacement character
            ++index;
        }
        else
        {
            emitCodePoint(current.unicode(), output);
            ++index;
        }
    }
}

// Resolve the encoding from an explicit override. Sets m_mode and
// m_metadata.format.encoding and returns the number of BOM bytes to skip. A
// negative return means "need more bytes" (an ambiguous BOM prefix).
qsizetype DocumentStreamDecoder::detectEncoding(const char *data, qsizetype size)
{
    switch (m_override)
    {
    case DocumentEncoding::Utf8:
    case DocumentEncoding::Utf8Bom:
        if (size >= 3 && static_cast<unsigned char>(data[0]) == 0xef
            && static_cast<unsigned char>(data[1]) == 0xbb
            && static_cast<unsigned char>(data[2]) == 0xbf)
        {
            recordEncoding(DocumentEncoding::Utf8Bom);
            m_metadata.hadBom = true;
            m_mode = Mode::Utf8;
            return 3;
        }
        if (size > 0 && static_cast<unsigned char>(data[0]) == 0xef && size < 3)
            return -1;
        m_mode = Mode::Utf8;
        recordEncoding(DocumentEncoding::Utf8);
        return 0;
    case DocumentEncoding::Utf16LittleEndian:
        if (size >= 2 && static_cast<unsigned char>(data[0]) == 0xff
            && static_cast<unsigned char>(data[1]) == 0xfe)
        {
            m_metadata.hadBom = true;
            m_mode = Mode::Utf16LittleEndian;
            recordEncoding(DocumentEncoding::Utf16LittleEndian);
            return 2;
        }
        if (size == 1 && static_cast<unsigned char>(data[0]) == 0xff)
            return -1;
        m_mode = Mode::Utf16LittleEndian;
        recordEncoding(DocumentEncoding::Utf16LittleEndian);
        return 0;
    case DocumentEncoding::Utf16BigEndian:
        if (size >= 2 && static_cast<unsigned char>(data[0]) == 0xfe
            && static_cast<unsigned char>(data[1]) == 0xff)
        {
            m_metadata.hadBom = true;
            m_mode = Mode::Utf16BigEndian;
            recordEncoding(DocumentEncoding::Utf16BigEndian);
            return 2;
        }
        if (size == 1 && static_cast<unsigned char>(data[0]) == 0xfe)
            return -1;
        m_mode = Mode::Utf16BigEndian;
        recordEncoding(DocumentEncoding::Utf16BigEndian);
        return 0;
    case DocumentEncoding::Latin1:
        m_mode = Mode::Latin1;
        recordEncoding(DocumentEncoding::Latin1);
        return 0;
    default:
        break;
    }
    if (DocumentFormatCodec::isLegacyEncoding(m_override))
    {
        m_mode = Mode::Legacy;
        recordEncoding(m_override);
        m_legacyDecoder = std::make_unique<LegacyTextCodec::Decoder>(m_override);
        return 0;
    }
    m_mode = Mode::Utf8;
    recordEncoding(DocumentEncoding::Utf8);
    return 0;
}

bool DocumentStreamDecoder::accept(const char *data, qsizetype size, QByteArray *output, QString *errorMessage)
{
    m_metadata.inputBytes += size;

    QByteArray combined;
    const char *p = data;
    qsizetype n = size;
    if (!m_carry.isEmpty())
    {
        combined.reserve(m_carry.size() + size);
        combined.append(m_carry);
        combined.append(data, size);
        m_carry.clear();
        p = combined.constData();
        n = combined.size();
    }

    if (m_mode == Mode::Detect)
    {
        if (m_overrideSet)
        {
            const qsizetype prefix = detectEncoding(p, n);
            if (prefix < 0)
            {
                m_carry = QByteArray(p, n);
                return true;
            }
            p += prefix;
            n -= prefix;
            if (n == 0)
                return true;
        }
        else
        {
            m_detectBuffer.append(p, n);
            if (m_detectBuffer.size() < DetectionWindowBytes)
                return true;
            return commitAutoDetect(output, errorMessage, false);
        }
    }

    return processMode(p, n, output, errorMessage);
}

bool DocumentStreamDecoder::processUtf8(const char *data, qsizetype size, QByteArray *output, QString *errorMessage)
{
    qsizetype index = 0;
    while (index < size)
    {
        const unsigned char first = static_cast<unsigned char>(data[index]);
        if (first <= 0x7f)
        {
            emitCodePoint(first, output);
            ++index;
            continue;
        }

        // A multi-byte character cannot be a line ending, so a pending CR is
        // definitely a lone CR.
        flushPendingCr(output);

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
            if (errorMessage)
                *errorMessage = QString::fromLatin1(InvalidUtf8Message);
            return false;
        }

        if (index + 1 + trailing > size)
        {
            m_carry = QByteArray(data + index, size - index);
            return true;
        }

        for (int k = 1; k <= trailing; ++k)
        {
            const unsigned char next = static_cast<unsigned char>(data[index + k]);
            if ((next & 0xc0) != 0x80)
            {
                if (errorMessage)
                    *errorMessage = QString::fromLatin1(InvalidUtf8Message);
                return false;
            }
            codePoint = (codePoint << 6) | (next & 0x3f);
        }
        if (codePoint < minimum || codePoint > 0x10ffff
            || (codePoint >= 0xd800 && codePoint <= 0xdfff))
        {
            if (errorMessage)
                *errorMessage = QString::fromLatin1(InvalidUtf8Message);
            return false;
        }

        appendOutput(output, data + index, 1 + trailing);
        index += 1 + trailing;
    }
    return true;
}

bool DocumentStreamDecoder::processUtf16(const char *data, qsizetype size, QByteArray *output, QString *errorMessage)
{
    const bool littleEndian = m_mode == Mode::Utf16LittleEndian;
    qsizetype index = 0;
    while (index + 1 < size)
    {
        const quint16 unit = littleEndian
            ? qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data) + index)
            : qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(data) + index);
        index += 2;

        if (m_highSurrogate)
        {
            if (unit >= 0xdc00 && unit <= 0xdfff)
            {
                const quint32 codePoint = 0x10000
                    + ((quint32(m_highSurrogateUnit) - 0xd800) << 10)
                    + (quint32(unit) - 0xdc00);
                m_highSurrogate = false;
                emitCodePoint(codePoint, output);
            }
            else
            {
                if (errorMessage)
                    *errorMessage = QString::fromLatin1(InvalidSurrogateMessage);
                return false;
            }
            continue;
        }

        if (unit >= 0xd800 && unit <= 0xdbff)
        {
            m_highSurrogate = true;
            m_highSurrogateUnit = unit;
        }
        else if (unit >= 0xdc00 && unit <= 0xdfff)
        {
            if (errorMessage)
                *errorMessage = QString::fromLatin1(InvalidSurrogateMessage);
            return false;
        }
        else
        {
            emitCodePoint(unit, output);
        }
    }

    if (index < size)
    {
        m_carry = QByteArray(data + index, size - index);
    }
    return true;
}

bool DocumentStreamDecoder::processLatin1(const char *data, qsizetype size, QByteArray *output)
{
    for (qsizetype index = 0; index < size; ++index)
        emitCodePoint(static_cast<unsigned char>(data[index]), output);
    return true;
}

bool DocumentStreamDecoder::processLegacy(const char *data, qsizetype size, QByteArray *output, QString *errorMessage)
{
    QString decoded;
    if (!m_legacyDecoder || !m_legacyDecoder->accept(data, size, &decoded, errorMessage))
        return false;
    emitDecodedText(decoded, output);
    return true;
}

bool DocumentStreamDecoder::processMode(const char *data, qsizetype size, QByteArray *output, QString *errorMessage)
{
    switch (m_mode)
    {
    case Mode::Utf8:
        return processUtf8(data, size, output, errorMessage);
    case Mode::Utf16LittleEndian:
    case Mode::Utf16BigEndian:
        return processUtf16(data, size, output, errorMessage);
    case Mode::Latin1:
        return processLatin1(data, size, output);
    case Mode::Legacy:
        return processLegacy(data, size, output, errorMessage);
    default:
        break;
    }
    return true;
}

// Commits the auto-detected encoding and decodes the buffered prefix. Called
// once the detect buffer reaches the detection window, or at end-of-input
// (atEnd). A trailing incomplete UTF-8 sequence is only tolerated when more
// input may still arrive.
bool DocumentStreamDecoder::commitAutoDetect(QByteArray *output, QString *errorMessage, bool atEnd)
{
    const QByteArray buffered = std::move(m_detectBuffer);
    m_detectBuffer.clear();

    DocumentEncoding encoding = DocumentEncoding::Utf8;
    const qsizetype prefix = DocumentFormatCodec::detectAutoEncoding(buffered, &encoding, !atEnd);

    switch (encoding)
    {
    case DocumentEncoding::Utf8:
        m_mode = Mode::Utf8;
        break;
    case DocumentEncoding::Utf8Bom:
        m_mode = Mode::Utf8;
        m_metadata.hadBom = true;
        break;
    case DocumentEncoding::Utf16LittleEndian:
        m_mode = Mode::Utf16LittleEndian;
        m_metadata.hadBom = true;
        break;
    case DocumentEncoding::Utf16BigEndian:
        m_mode = Mode::Utf16BigEndian;
        m_metadata.hadBom = true;
        break;
    default:
        m_mode = Mode::Legacy;
        m_legacyDecoder = std::make_unique<LegacyTextCodec::Decoder>(encoding);
        break;
    }
    recordEncoding(encoding);

    const char *data = buffered.constData() + prefix;
    const qsizetype size = buffered.size() - prefix;
    if (size == 0)
        return true;
    return processMode(data, size, output, errorMessage);
}

bool DocumentStreamDecoder::finish(QByteArray *output, QString *errorMessage)
{
    if (m_mode == Mode::Detect)
    {
        if (!m_overrideSet)
        {
            // End-of-input with an undecided encoding: run auto-detection on
            // whatever was buffered (possibly nothing, which resolves to UTF-8).
            if (!commitAutoDetect(output, errorMessage, true))
                return false;
        }
        else
        {
            const qsizetype prefix = detectEncoding("", 0);
            Q_UNUSED(prefix);
        }
    }

    if (!m_carry.isEmpty())
    {
        if (m_mode == Mode::Utf8)
        {
            if (errorMessage)
                *errorMessage = QString::fromLatin1(InvalidUtf8Message);
            return false;
        }
        if (m_mode == Mode::Utf16LittleEndian || m_mode == Mode::Utf16BigEndian)
        {
            if (errorMessage)
                *errorMessage = QString::fromLatin1(IncompleteUtf16Message);
            return false;
        }
    }

    if (m_highSurrogate)
    {
        if (errorMessage)
            *errorMessage = QString::fromLatin1(InvalidSurrogateMessage);
        return false;
    }

    if (m_mode == Mode::Legacy)
    {
        QString tail;
        if (!m_legacyDecoder || !m_legacyDecoder->finish(&tail, errorMessage))
            return false;
        emitDecodedText(tail, output);
    }

    flushPendingCr(output);

    const qsizetype kinds = qsizetype(m_crlf > 0) + qsizetype(m_lf > 0) + qsizetype(m_cr > 0);
    m_metadata.lineBreakCount = m_crlf + m_lf + m_cr;
    m_metadata.mixedLineEndings = kinds > 1;
    DocumentLineEnding ending = DocumentFormatCodec::defaultFormat().lineEnding;
    if (kinds == 1)
    {
        if (m_crlf > 0)
            ending = DocumentLineEnding::CrLf;
        else if (m_cr > 0)
            ending = DocumentLineEnding::Cr;
        else
            ending = DocumentLineEnding::Lf;
    }
    else if (kinds > 1)
    {
        ending = DocumentLineEnding::Mixed;
    }
    m_metadata.format.lineEnding = ending;
    return true;
}
