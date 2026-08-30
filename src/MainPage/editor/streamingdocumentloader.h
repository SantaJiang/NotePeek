#ifndef STREAMINGDOCUMENTLOADER_H
#define STREAMINGDOCUMENTLOADER_H

#include "editor/documentstreamdecoder.h"

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QString>

#include <memory>

// Streaming constants shared by the loader, the service and the tests so the
// bounded-memory guarantee is defined and asserted in one place.
constexpr qsizetype StreamReadChunkBytes = 2 * 1024 * 1024;
constexpr qsizetype StreamMaxQueuedBytes = 6 * 1024 * 1024;
constexpr int StreamMaxDrainChunksPerTick = 2;

// One decoded, LF-normalized UTF-8 chunk ready for the GUI thread to append.
struct DecodedDocumentChunk
{
    quint64 requestId = 0;
    quint64 sequence = 0;
    QByteArray utf8;
};

struct StreamingDocumentLoaderState;

// Streams a large file off the UI thread with bounded memory. The worker reads
// fixed-size chunks, decodes them incrementally and pushes decoded output into
// a bounded queue; a 0 ms drain timer on the GUI thread pulls 1-2 chunks per
// tick and emits chunkReady(). Backpressure (StreamMaxQueuedBytes) caps how far
// the worker can run ahead, so a 100 MB file never produces an unbounded
// number of queued chunks.
//
// One loader handles one request. All chunkReady()/finished()/failed()
// emissions happen on the loader's (GUI) thread.
class StreamingDocumentLoader : public QObject
{
    Q_OBJECT

public:
    explicit StreamingDocumentLoader(QObject *parent = nullptr);
    ~StreamingDocumentLoader() override;

    void start(quint64 requestId, const QString &filePath);
    // Stops the worker and discards queued chunks. Safe to call from any
    // thread; the loader must not be used after cancelAll().
    void cancelAll();
    bool isActive() const;
    // Peak number of bytes held in the decoded-chunk queue so far. Diagnostic:
    // read it after finished()/failed() (or cancelAll()) for a stable value.
    qint64 peakQueuedBytes() const;

signals:
    void chunkReady(quint64 requestId, quint64 sequence, const QByteArray &utf8);
    void finished(quint64 requestId, const StreamDocumentMetadata &metadata);
    void failed(quint64 requestId, const QString &errorMessage);

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void drain();
    void finalize();

    std::shared_ptr<StreamingDocumentLoaderState> m_state;
    int m_timerId = 0;
};

#endif // STREAMINGDOCUMENTLOADER_H
