#include "editor/documentpersistenceservice.h"

#include "foundation/backgroundworkscheduler.h"
#include "editor/documentio.h"
#include "editor/documentsavecoordinator.h"
#include "editor/streamingdocumentloader.h"
#include <NotePeek/lexerfactory.h>
#include "foundation/performanceconstants.h"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>
DocumentPersistenceService::DocumentPersistenceService(DocumentSaveCoordinator *saveCoordinator,
                                                       QObject *parent)
    : QObject(parent)
    , m_saveCoordinator(saveCoordinator)
{
    Q_ASSERT(m_saveCoordinator);
}

void DocumentPersistenceService::loadFileAsync(const DocumentLoadRequest &request)
{
    const QFileInfo fileInfo(request.filePath);
    const bool largeFile = fileInfo.size() > PerformanceConstants::LargeFileSize;
    if (largeFile)
    {
        startStreamingLoad(request);
        return;
    }

    auto *watcher = new QFutureWatcher<DocumentLoadResult>(this);
    connect(watcher, &QFutureWatcher<DocumentLoadResult>::finished, this, [this, watcher]() {
        const DocumentLoadResult result = watcher->result();
        watcher->deleteLater();
        emit fileLoaded(result);
    });

    watcher->setFuture(QtConcurrent::run(BackgroundWorkScheduler::ioPool(), [request]() {
        DocumentLoadResult result;
        result.requestId = request.requestId;
        result.filePath = request.filePath;
        result.fileSize = QFileInfo(request.filePath).size();
        result.largeFile = result.fileSize > PerformanceConstants::LargeFileSize;
        if (result.fileSize > PerformanceConstants::MaximumEditableFileSize)
        {
            result.errorMessage = tr("The file is larger than the supported 256 MB editing limit.");
            return result;
        }
        result.success = DocumentIO::loadEditorFile(request.filePath,
                                                    &result.content,
                                                    &result.format,
                                                    &result.errorMessage);
        if (result.success && !result.largeFile)
        {
            result.abnormallyLongLine = PerformanceConstants::containsAbnormallyLongLine(result.content);
            result.largeFile = result.largeFile || result.abnormallyLongLine;
            if (!result.largeFile)
            {
                LexerFactory::Language language = LexerFactory::languageFromFilePath(request.filePath);
                if (language == LexerFactory::Unknown)
                    language = LexerFactory::languageFromContent(result.content);
                result.detectedLanguage = int(language);
            }
        }
        return result;
    }));
}

void DocumentPersistenceService::startStreamingLoad(const DocumentLoadRequest &request)
{
    // The whole-file path rejects oversized files before reading them; the
    // streaming path must apply the same limit or a >256 MB file would be
    // streamed in full.
    if (QFileInfo(request.filePath).size() > PerformanceConstants::MaximumEditableFileSize)
    {
        emit streamFailed(request.requestId,
                          tr("The file is larger than the supported 256 MB editing limit."));
        return;
    }

    auto *loader = new StreamingDocumentLoader(this);
    m_streamingLoads.insert(request.requestId, loader);

    connect(loader, &StreamingDocumentLoader::chunkReady,
            this, &DocumentPersistenceService::streamChunkReady);
    connect(loader, &StreamingDocumentLoader::finished, this,
            [this, requestId = request.requestId](quint64, const StreamDocumentMetadata &metadata) {
        emit streamFinished(requestId, metadata);
        finishStreamingLoad(requestId);
    });
    connect(loader, &StreamingDocumentLoader::failed, this,
            [this, requestId = request.requestId](quint64, const QString &errorMessage) {
        emit streamFailed(requestId, errorMessage);
        finishStreamingLoad(requestId);
    });

    loader->start(request.requestId, request.filePath);
}

void DocumentPersistenceService::finishStreamingLoad(quint64 requestId)
{
    if (StreamingDocumentLoader *loader = m_streamingLoads.take(requestId))
        loader->deleteLater();
}

void DocumentPersistenceService::cancelLoad(quint64 requestId)
{
    if (StreamingDocumentLoader *loader = m_streamingLoads.take(requestId))
    {
        loader->cancelAll();
        loader->deleteLater();
    }
}

void DocumentPersistenceService::saveFileAsync(const DocumentSaveRequest &request)
{
    m_saveCoordinator->enqueue(this, request);
}

void DocumentPersistenceService::waitForPendingSavePath(const QString &filePath)
{
    m_saveCoordinator->waitForPath(filePath);
}
