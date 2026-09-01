#include "editor/legacytextcodec.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <iconv.h>
#endif

namespace {

#ifdef Q_OS_WIN
unsigned windowsCodePage(DocumentEncoding encoding)
{
    switch (encoding)
    {
    case DocumentEncoding::Gb18030: return 54936;
    case DocumentEncoding::Big5: return 950;
    case DocumentEncoding::ShiftJis: return 932;
    case DocumentEncoding::EucJp: return 20932;
    case DocumentEncoding::EucKr: return 949;
    case DocumentEncoding::Windows1250: return 1250;
    case DocumentEncoding::Windows1251: return 1251;
    case DocumentEncoding::Windows1252: return 1252;
    case DocumentEncoding::Windows1253: return 1253;
    case DocumentEncoding::Windows1254: return 1254;
    case DocumentEncoding::Windows1255: return 1255;
    case DocumentEncoding::Windows1256: return 1256;
    case DocumentEncoding::Windows1257: return 1257;
    case DocumentEncoding::Windows1258: return 1258;
    case DocumentEncoding::Iso88592: return 28592;
    case DocumentEncoding::Iso88595: return 28595;
    case DocumentEncoding::Iso88597: return 28597;
    case DocumentEncoding::Iso885915: return 28605;
    case DocumentEncoding::Koi8R: return 20866;
    case DocumentEncoding::Koi8U: return 21866;
    case DocumentEncoding::Tis620: return 874;
    default: return 0;
    }
}

// Longest multi-byte sequence for a code page, used to bound how many trailing
// bytes must be held back when a sequence is split across chunks.
int maxSequenceBytes(unsigned codePage)
{
    switch (codePage)
    {
    case 54936: return 4;   // GB18030
    case 20932: return 3;   // EUC-JP (0x8F + two bytes for JIS X 0212)
    case 932:
    case 936:
    case 950:
    case 949: return 2;     // DBCS code pages
    default: return 1;      // single-byte code pages
    }
}
#else
// Candidate iconv names per encoding, tried in order so minor libc naming
// differences (glibc vs. BSD/libiconv) do not matter.
QList<QByteArray> iconvNames(DocumentEncoding encoding)
{
    switch (encoding)
    {
    case DocumentEncoding::Gb18030: return {QByteArrayLiteral("GB18030")};
    case DocumentEncoding::Big5: return {QByteArrayLiteral("BIG5"), QByteArrayLiteral("BIG-5")};
    case DocumentEncoding::ShiftJis: return {QByteArrayLiteral("SHIFT_JIS"),
                                             QByteArrayLiteral("SJIS"),
                                             QByteArrayLiteral("SHIFT-JIS")};
    case DocumentEncoding::EucJp: return {QByteArrayLiteral("EUC-JP")};
    case DocumentEncoding::EucKr: return {QByteArrayLiteral("EUC-KR")};
    case DocumentEncoding::Windows1250: return {QByteArrayLiteral("WINDOWS-1250"), QByteArrayLiteral("CP1250")};
    case DocumentEncoding::Windows1251: return {QByteArrayLiteral("WINDOWS-1251"), QByteArrayLiteral("CP1251")};
    case DocumentEncoding::Windows1252: return {QByteArrayLiteral("WINDOWS-1252"), QByteArrayLiteral("CP1252")};
    case DocumentEncoding::Windows1253: return {QByteArrayLiteral("WINDOWS-1253"), QByteArrayLiteral("CP1253")};
    case DocumentEncoding::Windows1254: return {QByteArrayLiteral("WINDOWS-1254"), QByteArrayLiteral("CP1254")};
    case DocumentEncoding::Windows1255: return {QByteArrayLiteral("WINDOWS-1255"), QByteArrayLiteral("CP1255")};
    case DocumentEncoding::Windows1256: return {QByteArrayLiteral("WINDOWS-1256"), QByteArrayLiteral("CP1256")};
    case DocumentEncoding::Windows1257: return {QByteArrayLiteral("WINDOWS-1257"), QByteArrayLiteral("CP1257")};
    case DocumentEncoding::Windows1258: return {QByteArrayLiteral("WINDOWS-1258"), QByteArrayLiteral("CP1258")};
    case DocumentEncoding::Iso88592: return {QByteArrayLiteral("ISO-8859-2")};
    case DocumentEncoding::Iso88595: return {QByteArrayLiteral("ISO-8859-5")};
    case DocumentEncoding::Iso88597: return {QByteArrayLiteral("ISO-8859-7")};
    case DocumentEncoding::Iso885915: return {QByteArrayLiteral("ISO-8859-15")};
    case DocumentEncoding::Koi8R: return {QByteArrayLiteral("KOI8-R")};
    case DocumentEncoding::Koi8U: return {QByteArrayLiteral("KOI8-U")};
    case DocumentEncoding::Tis620: return {QByteArrayLiteral("TIS-620"), QByteArrayLiteral("TIS620")};
    default: return {};
    }
}

iconv_t openIconv(DocumentEncoding encoding, bool toUnicode)
{
    const QList<QByteArray> names = iconvNames(encoding);
    for (const QByteArray &name : names)
    {
        // toUnicode: convert from the legacy encoding to UTF-8.
        const iconv_t cd = toUnicode
            ? iconv_open("UTF-8", name.constData())
            : iconv_open(name.constData(), "UTF-8");
        if (cd != reinterpret_cast<iconv_t>(-1))
            return cd;
    }
    return reinterpret_cast<iconv_t>(-1);
}

int utf8SequenceLength(unsigned char lead)
{
    if (lead < 0x80)
        return 1;
    if ((lead & 0xe0) == 0xc0)
        return 2;
    if ((lead & 0xf0) == 0xe0)
        return 3;
    if ((lead & 0xf8) == 0xf0)
        return 4;
    return 1;
}

// Converts `size` bytes at `data` through `cd`. On success appends the result
// to `*output` and returns true. When `substituteInvalid` is false the first
// invalid/unrepresentable input byte fails the conversion; when true it is
// replaced with '?' and conversion continues.
bool iconvConvert(iconv_t cd, const char *data, size_t size, QByteArray *output,
                  bool substituteInvalid)
{
    const char *inbuf = data;
    size_t inleft = size;

    QByteArray buffer;
    buffer.resize(size * 2 + 64);
    size_t outUsed = 0;

    while (inleft > 0)
    {
        char *outbuf = buffer.data() + outUsed;
        size_t outleft = buffer.size() - outUsed;
        const size_t result = iconv(cd, const_cast<char **>(&inbuf), &inleft, &outbuf, &outleft);
        outUsed = size_t(outbuf - buffer.data());

        if (result != size_t(-1))
        {
            output->append(buffer.constData(), qsizetype(outUsed));
            return true;
        }

        switch (errno)
        {
        case E2BIG:
            buffer.resize(buffer.size() * 2);
            continue;
        case EILSEQ:
        case EINVAL:
            if (!substituteInvalid)
                return false;
            output->append(buffer.constData(), qsizetype(outUsed));
            outUsed = 0;
            output->append('?');
            {
                // Skip the whole unrepresentable UTF-8 sequence, not just its
                // lead byte, so one character becomes one '?'.
                size_t advance = size_t(utf8SequenceLength(static_cast<unsigned char>(*inbuf)));
                if (advance > inleft)
                    advance = inleft;
                inbuf += advance;
                inleft -= advance;
            }
            continue;
        default:
            return false;
        }
    }

    output->append(buffer.constData(), qsizetype(outUsed));
    return true;
}
#endif

QString legacyError(DocumentEncoding encoding)
{
    return QStringLiteral("The file contains invalid %1 data.")
        .arg(DocumentFormatCodec::encodingName(encoding));
}

} // namespace

bool LegacyTextCodec::decode(const QByteArray &input, DocumentEncoding encoding,
                             QString *output, QString *errorMessage)
{
    if (output)
        output->clear();

    if (input.isEmpty())
        return true;

#ifdef Q_OS_WIN
    const unsigned codePage = windowsCodePage(encoding);
    if (codePage == 0)
    {
        if (errorMessage)
            *errorMessage = legacyError(encoding);
        return false;
    }
    const int needed = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS,
                                           input.constData(), int(input.size()),
                                           nullptr, 0);
    if (needed == 0)
    {
        if (errorMessage)
            *errorMessage = legacyError(encoding);
        return false;
    }
    QString result(needed, QChar(0));
    MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, input.constData(),
                        int(input.size()), reinterpret_cast<LPWSTR>(result.data()),
                        needed);
    if (output)
        *output = result;
    return true;
#else
    const iconv_t cd = openIconv(encoding, true);
    if (cd == reinterpret_cast<iconv_t>(-1))
    {
        if (errorMessage)
            *errorMessage = legacyError(encoding);
        return false;
    }

    QByteArray utf8;
    const bool ok = iconvConvert(cd, input.constData(), size_t(input.size()), &utf8, false);
    iconv_close(cd);
    if (!ok)
    {
        if (errorMessage)
            *errorMessage = legacyError(encoding);
        return false;
    }
    if (output)
        *output = QString::fromUtf8(utf8.constData(), utf8.size());
    return true;
#endif
}

QByteArray LegacyTextCodec::encode(const QString &input, DocumentEncoding encoding)
{
#ifdef Q_OS_WIN
    const unsigned codePage = windowsCodePage(encoding);
    if (codePage == 0 || input.isEmpty())
        return QByteArray();
    const int needed = WideCharToMultiByte(
        codePage, 0, reinterpret_cast<LPCWCH>(input.utf16()), int(input.size()),
        nullptr, 0, nullptr, nullptr);
    QByteArray result(needed, Qt::Uninitialized);
    WideCharToMultiByte(codePage, 0, reinterpret_cast<LPCWCH>(input.utf16()),
                        int(input.size()), result.data(), needed, nullptr, nullptr);
    return result;
#else
    const iconv_t cd = openIconv(encoding, false);
    if (cd == reinterpret_cast<iconv_t>(-1))
        return QByteArray();

    const QByteArray utf8 = input.toUtf8();
    QByteArray output;
    iconvConvert(cd, utf8.constData(), size_t(utf8.size()), &output, true);
    iconv_close(cd);
    return output;
#endif
}

bool LegacyTextCodec::canRepresent(const QString &text, DocumentEncoding encoding)
{
    const QByteArray encoded = encode(text, encoding);
    QString round;
    if (!decode(encoded, encoding, &round, nullptr))
        return false;
    return round == text;
}

struct LegacyTextCodec::Decoder::Impl
{
    DocumentEncoding encoding = DocumentEncoding::Utf8;
#ifdef Q_OS_WIN
    unsigned codePage = 0;
#else
    iconv_t cd = reinterpret_cast<iconv_t>(-1);
#endif
    QByteArray carry;
};

LegacyTextCodec::Decoder::Decoder(DocumentEncoding encoding)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->encoding = encoding;
#ifdef Q_OS_WIN
    m_impl->codePage = windowsCodePage(encoding);
#else
    m_impl->cd = openIconv(encoding, true);
#endif
}

LegacyTextCodec::Decoder::~Decoder()
{
#ifndef Q_OS_WIN
    if (m_impl->cd != reinterpret_cast<iconv_t>(-1))
        iconv_close(m_impl->cd);
#endif
}

bool LegacyTextCodec::Decoder::accept(const char *data, qsizetype size,
                                      QString *output, QString *errorMessage)
{
    if (output)
        output->clear();

    QByteArray combined;
    const char *p = data;
    qsizetype n = size;
    if (!m_impl->carry.isEmpty())
    {
        combined.reserve(m_impl->carry.size() + size);
        combined.append(m_impl->carry);
        combined.append(data, size);
        m_impl->carry.clear();
        p = combined.constData();
        n = combined.size();
    }

    if (n == 0)
        return true;

#ifdef Q_OS_WIN
    const unsigned codePage = m_impl->codePage;
    if (codePage == 0)
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("The requested text encoding is not supported.");
        return false;
    }

    // MultiByteToWideChar is not incremental, so a multi-byte sequence split
    // across chunks must be held back. Try decoding the tail with increasing
    // amounts held back; the first attempt that succeeds identifies the split
    // point. If even holding the maximum sequence length fails, the data is
    // genuinely invalid.
    const int maxSeq = maxSequenceBytes(codePage);
    const int maxHold = qMin<int>(maxSeq - 1, int(n));
    int hold = 0;
    int needed = 0;
    for (; hold <= maxHold; ++hold)
    {
        needed = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, p,
                                     int(n) - hold, nullptr, 0);
        if (needed != 0)
            break;
    }
    if (needed == 0)
    {
        if (errorMessage)
            *errorMessage = legacyError(m_impl->encoding);
        return false;
    }

    m_impl->carry = QByteArray(p + (n - hold), hold);

    QString result(needed, QChar(0));
    MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, p, int(n) - hold,
                        reinterpret_cast<LPWSTR>(result.data()), needed);
    if (output)
        *output = result;
    return true;
#else
    if (m_impl->cd == reinterpret_cast<iconv_t>(-1))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("The requested text encoding is not supported.");
        return false;
    }

    QByteArray utf8;
    utf8.resize(qMax<qsizetype>(64, n * 2 + 16));
    qsizetype outUsed = 0;

    char *inbuf = const_cast<char *>(p);
    size_t inleft = size_t(n);
    while (inleft > 0)
    {
        char *outbuf = utf8.data() + outUsed;
        size_t outleft = size_t(utf8.size() - outUsed);
        const size_t result = iconv(m_impl->cd, &inbuf, &inleft, &outbuf, &outleft);
        outUsed = outbuf - utf8.data();
        if (result != size_t(-1))
            break;
        if (errno == E2BIG)
        {
            utf8.resize(utf8.size() * 2);
            continue;
        }
        if (errno == EINVAL)
        {
            // Incomplete sequence at the end of the buffer: keep the remaining
            // bytes for the next chunk.
            m_impl->carry = QByteArray(inbuf, qsizetype(inleft));
            inleft = 0;
            break;
        }
        if (errorMessage)
            *errorMessage = legacyError(m_impl->encoding);
        return false;
    }

    if (output)
        *output = QString::fromUtf8(utf8.constData(), outUsed);
    return true;
#endif
}

bool LegacyTextCodec::Decoder::finish(QString *output, QString *errorMessage)
{
    if (output)
        output->clear();

    if (!m_impl->carry.isEmpty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("The file contains an incomplete character sequence.");
        return false;
    }

#ifdef Q_OS_WIN
    return true;
#else
    if (m_impl->cd == reinterpret_cast<iconv_t>(-1))
        return true;

    // Flush any pending shift state (stateless codecs emit nothing).
    QByteArray utf8;
    utf8.resize(64);
    char *outbuf = utf8.data();
    size_t outleft = size_t(utf8.size());
    const size_t result = iconv(m_impl->cd, nullptr, nullptr, &outbuf, &outleft);
    if (result == size_t(-1))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("The file contains invalid data.");
        return false;
    }
    if (output)
        *output = QString::fromUtf8(utf8.constData(), qsizetype(outbuf - utf8.data()));
    return true;
#endif
}
