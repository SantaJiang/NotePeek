#ifndef DOCUMENTSTREAMDECODER_H
#define DOCUMENTSTREAMDECODER_H

#include "editor/documentformat.h"
#include "editor/legacytextcodec.h"

#include <QByteArray>
#include <QMetaType>
#include <QString>

#include <memory>

// Metadata gathered while streaming a document. `format` carries the same
// encoding/line-ending classification the whole-file DocumentFormatCodec
// decoder would have produced.
struct StreamDocumentMetadata
{
    DocumentFormat format;
    qint64 inputBytes = 0;
    qint64 outputUtf8Bytes = 0;
    qint64 lineBreakCount = 0;
    bool mixedLineEndings = false;
    bool hadBom = false;
};

Q_DECLARE_METATYPE(StreamDocumentMetadata)
// Incrementally decodes a file's raw bytes into the UTF-8 / LF-normalized
// representation consumed by Scintilla. Handles BOM detection, UTF-8, UTF-16
// (LE/BE) and Latin-1 across arbitrary chunk boundaries and reports the exact
// line-ending statistics that DocumentFormatCodec would have produced, so a
// streamed load is byte-for-byte equivalent to the whole-file decoder.
//
// Usage: optionally set an encoding override, then call accept() for each raw
// input chunk (any size, including 1 byte) and finish() once at end-of-input.
// The decoder holds only a constant amount of carry state between calls; every
// input byte is visited once.
class DocumentStreamDecoder
{
public:
    DocumentStreamDecoder();

    // Force a specific encoding (reopen-with-encoding). A matching BOM is
    // still consumed when present. Must be called before the first accept().
    void setEncodingOverride(DocumentEncoding encoding);

    // Decode `size` bytes of `data`, appending newly produced output to
    // `*output`. Returns false and sets `*errorMessage` on invalid input.
    bool accept(const char *data, qsizetype size, QByteArray *output, QString *errorMessage);

    // Signal end-of-input and flush residual state (a trailing CR, an odd
    // UTF-16 byte or an incomplete UTF-8 sequence, which are errors). Returns
    // false on error.
    bool finish(QByteArray *output, QString *errorMessage);

    const StreamDocumentMetadata &metadata() const { return m_metadata; }

private:
    enum class Mode
    {
        Detect,
        Utf8,
        Utf16LittleEndian,
        Utf16BigEndian,
        Latin1,
        Legacy
    };

    bool processUtf8(const char *data, qsizetype size, QByteArray *output, QString *errorMessage);
    bool processUtf16(const char *data, qsizetype size, QByteArray *output, QString *errorMessage);
    bool processLatin1(const char *data, qsizetype size, QByteArray *output);
    bool processLegacy(const char *data, qsizetype size, QByteArray *output, QString *errorMessage);
    bool processMode(const char *data, qsizetype size, QByteArray *output, QString *errorMessage);
    bool commitAutoDetect(QByteArray *output, QString *errorMessage, bool atEnd);
    qsizetype detectEncoding(const char *data, qsizetype size);

    void emitCodePoint(quint32 codePoint, QByteArray *output);
    void emitDecodedText(const QString &decoded, QByteArray *output);
    void appendOutput(QByteArray *output, char byte);
    void appendOutput(QByteArray *output, const char *data, qsizetype size);
    void flushPendingCr(QByteArray *output);
    void recordEncoding(DocumentEncoding encoding);

    Mode m_mode = Mode::Detect;
    bool m_overrideSet = false;
    DocumentEncoding m_override = DocumentEncoding::Utf8;

    QByteArray m_detectBuffer;          // Buffered prefix while auto-detecting.
    std::unique_ptr<LegacyTextCodec::Decoder> m_legacyDecoder;
    QByteArray m_carry;             // BOM prefix / incomplete UTF-8 / odd UTF-16 byte.
    bool m_pendingCr = false;       // Last decoded character was a bare '\r'.
    bool m_highSurrogate = false;   // A UTF-16 high surrogate is pending.
    quint16 m_highSurrogateUnit = 0;

    qint64 m_crlf = 0;
    qint64 m_lf = 0;
    qint64 m_cr = 0;

    StreamDocumentMetadata m_metadata;
};

#endif // DOCUMENTSTREAMDECODER_H
