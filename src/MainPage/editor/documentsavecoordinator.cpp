#include "editor/documentsavecoordinator.h"

#include "editor/documentio.h"
#include "editor/backgroundworkscheduler.h"
#include "editor/documentpath.h"

#include <QEventLoop>
#include <QFutureWatcher>
#include <QtConcurrent>

DocumentSaveCoordinator::DocumentSaveCoordinator(QObject *parent)
    : QObject(parent)
{
}

void DocumentSaveCoordinator::enqueue(DocumentPersistenceService *receiver,
                                      const DocumentSaveRequest &request)
{
    const QString path = DocumentPath::identityKey(request.filePath);
    QQueue<Job> &queue = m_queues[path];
    Job superseded;
    bool hasSupersededJob = false;

    if (request.documentId != 0)
    {
        for (int i = queue.size() - 1; i >= 0; --i)
        {
            if (queue.at(i).request.documentId == request.documentId)
            {
                superseded = queue.takeAt(i);
                hasSupersededJob = true;
                break;
            }
        }
    }

    queue.enqueue({receiver, request});
    startNext(path);

    if (hasSupersededJob && superseded.receiver)
    {
        DocumentSaveResult result;
        result.requestId = superseded.request.requestId;
        result.filePath = superseded.request.filePath;
        result.success = true;
        result.superseded = true;
        emit superseded.receiver->fileSaved(result);
    }
}

void DocumentSaveCoordinator::waitForPath(const QString &filePath)
{
    const QString path = DocumentPath::identityKey(filePath);
    if (!m_activePaths.contains(path) && m_queues.value(path).isEmpty())
        return;

    QEventLoop loop;
    m_waiters[path].append(&loop);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
}

quint64 DocumentSaveCoordinator::beginDocumentSave(void *documentKey,
                                                    const QString &filePath)
{
    const quint64 generation = m_nextGeneration++;
    m_latestDocumentGenerations.insert(documentKey, generation);
    m_latestDocumentPaths.insert(documentKey, filePath);
    return generation;
}

bool DocumentSaveCoordinator::isLatestDocumentSave(void *documentKey, quint64 generation) const
{
    return documentKey && m_latestDocumentGenerations.value(documentKey) == generation;
}

bool DocumentSaveCoordinator::hasOutstandingDocumentSave(void *documentKey) const
{
    return documentKey && m_latestDocumentGenerations.contains(documentKey);
}

QString DocumentSaveCoordinator::latestDocumentSavePath(void *documentKey) const
{
    return documentKey ? m_latestDocumentPaths.value(documentKey) : QString();
}

void DocumentSaveCoordinator::finishDocumentSave(void *documentKey, quint64 generation)
{
    if (isLatestDocumentSave(documentKey, generation))
    {
        m_latestDocumentGenerations.remove(documentKey);
        m_latestDocumentPaths.remove(documentKey);
    }
}

void DocumentSaveCoordinator::startNext(const QString &path)
{
    if (m_activePaths.contains(path))
        return;

    QQueue<Job> &queue = m_queues[path];
    if (queue.isEmpty())
    {
        m_queues.remove(path);
        const QList<QPointer<QEventLoop>> waiters = m_waiters.take(path);
        for (const QPointer<QEventLoop> &waiter : waiters)
        {
            if (waiter)
                waiter->quit();
        }
        return;
    }

    const Job job = queue.dequeue();
    m_activePaths.insert(path);
    auto *watcher = new QFutureWatcher<DocumentSaveResult>(this);
    connect(watcher, &QFutureWatcher<DocumentSaveResult>::finished,
            this, [this, watcher, path, job]() {
        const DocumentSaveResult result = watcher->result();
        watcher->deleteLater();
        m_activePaths.remove(path);
        startNext(path);
        if (job.receiver)
            emit job.receiver->fileSaved(result);
    });
    watcher->setFuture(QtConcurrent::run(BackgroundWorkScheduler::ioPool(), [request = job.request]() {
        DocumentSaveResult result;
        result.requestId = request.requestId;
        result.filePath = request.filePath;
        result.success = DocumentIO::saveUtf8File(request.filePath,
                                                  request.utf8Content,
                                                  &result.errorMessage);
        return result;
    }));
}
