#ifndef LEGACYTEXTCODEC_H
#define LEGACYTEXTCODEC_H

#include "editor/documentformat.h"

#include <QByteArray>
#include <QString>

#include <memory>

// Platform-backed codec for legacy text encodings (GB18030, Big5, Shift-JIS,
// EUC-*, the Windows-125x / ISO-8859-x single-byte code pages, KOI8, TIS-620).
// Qt 6 core has no codec for these, so Unix uses iconv(3) and Windows uses
// MultiByteToWideChar / WideCharToMultiByte. This keeps the codec dependency in
// one translation unit and lets the rest of the codebase treat every legacy
// encoding uniformly.
namespace LegacyTextCodec {

// Decodes `input` as `encoding`. Returns false (and sets *errorMessage) when
// the byte sequence is not valid for that encoding.
bool decode(const QByteArray &input, DocumentEncoding encoding,
            QString *output, QString *errorMessage);

// Encodes `input` as `encoding`. Unrepresentable characters are replaced with
// '?' (matching QString::toLatin1 semantics), so this never fails.
QByteArray encode(const QString &input, DocumentEncoding encoding);

// True when every character in `text` round-trips through `encoding` without
// substitution.
bool canRepresent(const QString &text, DocumentEncoding encoding);

// Incremental decoder mirroring decode() so large files can be decoded chunk
// by chunk without materialising the whole file. Feed bytes with accept(); call
// finish() once at end-of-input. A partial multi-byte sequence at a chunk
// boundary is held until the rest arrives; finish() reports a trailing partial
// sequence as an error.
class Decoder
{
public:
    explicit Decoder(DocumentEncoding encoding);
    ~Decoder();
    Decoder(const Decoder &) = delete;
    Decoder &operator=(const Decoder &) = delete;

    // Decodes `size` bytes of `data`, appending newly decoded text to
    // `*output`. Returns false (and sets *errorMessage) on invalid input.
    bool accept(const char *data, qsizetype size, QString *output, QString *errorMessage);

    // Flushes any residual state. Returns false if a partial sequence remains.
    bool finish(QString *output, QString *errorMessage);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace LegacyTextCodec

#endif // LEGACYTEXTCODEC_H
