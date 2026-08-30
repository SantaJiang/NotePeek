#include "editor/streamingdocumentloader.h"

#include "foundation/backgroundworkscheduler.h"

#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTimerEvent>
#include <QWaitCondition>
#include <QtConcurrent>

#include <atomic>
#include <vector>

struct StreamingDocumentLoaderState
{
    quint64 requestId = 0;
    QString filePath;
    std::atomic<bool> canceled{false};

    QMutex mutex;
    QWaitCondition notFull;
    QWaitCondition notEmpty;
    QQueue<DecodedDocumentChunk> chunks;
    qint64 queuedBytes = 0;
    std::atomic<qint64> peakQueuedBytes{0};

    bool workerDone = false;
    bool workerError = false;
    QString errorMessage;
    StreamDocumentMetadata metadata;
};

namespace {

void pushChunk(const std::shared_ptr<StreamingDocumentLoaderState> &state,
               QByteArray data, quint64 sequence, bool *ok)
{
    QMutexLocker lock(&state->mutex);
    while (state->queuedBytes + data.size() > StreamMaxQueuedBytes && !state->canceled.load())
        state->notFull.wait(&state->mutex);
    if (state->canceled.load())
    {
        *ok = false;
        return;
    }

    DecodedDocumentChunk chunk;
    chunk.requestId = state->requestId;
    chunk.sequence = sequence;
    chunk.utf8 = std::move(data);
    state->queuedBytes += chunk.utf8.size();
    state->chunks.enqueue(std::move(chunk));
    state->peakQueuedBytes.store(qMax(state->peakQueuedBytes.load(), state->queuedBytes));
    state->notEmpty.wakeAll();
}

void runStreamingWorker(std::shared_ptr<StreamingDocumentLoaderState> state)
{
    QFile file(state->filePath);
    bool ok = true;
    QString error;

    if (!file.open(QIODevice::ReadOnly))
    {
        error = file.errorString();
        ok = false;
    }

    DocumentStreamDecoder decoder;
    QByteArray buffer(StreamReadChunkBytes, Qt::Uninitialized);
    quint64 sequence = 0;

    while (ok)
    {
        if (state->canceled.load())
        {
            ok = false;
            break;
        }
        const qint64 read = file.read(buffer.data(), StreamReadChunkBytes);
        if (read < 0)
        {
            error = file.errorString();
            ok = false;
            break;
        }
        if (read == 0)
            break;

        QByteArray output;
        if (!decoder.accept(buffer.constData(), read, &output, &error))
        {
            ok = false;
            break;
        }

        qsizetype pos = 0;
        while (pos < output.size())
        {
            if (state->canceled.load())
            {
                ok = false;
                break;
            }
            const qsizetype piece = qMin(StreamReadChunkBytes, output.size() - pos);
            pushChunk(state, output.mid(pos, piece), sequence++, &ok);
            if (!ok)
                break;
            pos += piece;
        }
    }

    if (ok)
    {
        QByteArray finalOutput;
        if (!decoder.finish(&finalOutput, &error))
        {
            ok = false;
        }
        else if (!finalOutput.isEmpty())
        {
            pushChunk(state, std::move(finalOutput), sequence++, &ok);
        }
    }

    QMutexLocker lock(&state->mutex);
    state->workerError = !ok;
    state->errorMessage = error;
    if (ok)
        state->metadata = decoder.metadata();
    state->workerDone = true;
    state->notEmpty.wakeAll();
}

} // namespace

StreamingDocumentLoader::StreamingDocumentLoader(QObject *parent)
    : QObject(parent)
    , m_state(std::make_shared<StreamingDocumentLoaderState>())
{
}

StreamingDocumentLoader::~StreamingDocumentLoader()
{
    cancelAll();
}

void StreamingDocumentLoader::start(quint64 requestId, const QString &filePath)
{
    m_state->requestId = requestId;
    m_state->filePath = filePath;

    [[maybe_unused]] const auto future = QtConcurrent::run(
        BackgroundWorkScheduler::ioPool(),
        [state = m_state]() { runStreamingWorker(state); });

    // Drain the queue on the GUI thread as decoded chunks become available.
    m_timerId = startTimer(0);
}

void StreamingDocumentLoader::cancelAll()
{
    m_state->canceled.store(true);
    {
        QMutexLocker lock(&m_state->mutex);
        m_state->notFull.wakeAll();
        m_state->notEmpty.wakeAll();
    }
    if (m_timerId != 0)
    {
        killTimer(m_timerId);
        m_timerId = 0;
    }
}

bool StreamingDocumentLoader::isActive() const
{
    return m_timerId != 0;
}

qint64 StreamingDocumentLoader::peakQueuedBytes() const
{
    return m_state->peakQueuedBytes.load();
}

void StreamingDocumentLoader::drain()
{
    std::vector<DecodedDocumentChunk> taken;
    {
        QMutexLocker lock(&m_state->mutex);
        int drained = 0;
        while (!m_state->chunks.isEmpty() && drained < StreamMaxDrainChunksPerTick)
        {
            taken.push_back(m_state->chunks.dequeue());
            m_state->queuedBytes -= taken.back().utf8.size();
            ++drained;
        }
        m_state->notFull.wakeAll();
    }

    for (const DecodedDocumentChunk &chunk : taken)
        emit chunkReady(chunk.requestId, chunk.sequence, chunk.utf8);

    bool done = false;
    {
        QMutexLocker lock(&m_state->mutex);
        done = m_state->workerDone && m_state->chunks.isEmpty();
    }
    if (done)
        finalize();
}

void StreamingDocumentLoader::finalize()
{
    if (m_timerId != 0)
    {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    if (m_state->workerError)
        emit failed(m_state->requestId, m_state->errorMessage);
    else
        emit finished(m_state->requestId, m_state->metadata);
}

void StreamingDocumentLoader::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId)
        drain();
    else
        QObject::timerEvent(event);
}
