#include "editor/documentencodingreloadservice.h"

#include "editor/documentio.h"
#include "foundation/backgroundworkscheduler.h"
#include "foundation/performanceconstants.h"
#include <NotePeek/lexerfactory.h>

#include <QFutureWatcher>
#include <QtConcurrent>

DocumentEncodingReloadService::DocumentEncodingReloadService(QObject *parent)
    : QObject(parent)
{
}

void DocumentEncodingReloadService::reloadFileAsync(
    const DocumentEncodingReloadRequest &request)
{
    auto *watcher = new QFutureWatcher<DocumentEncodingReloadResult>(this);
    connect(watcher, &QFutureWatcher<DocumentEncodingReloadResult>::finished,
            this, [this, watcher]() {
        const DocumentEncodingReloadResult result = watcher->result();
        watcher->deleteLater();
        emit fileReloaded(result);
    });

    watcher->setFuture(QtConcurrent::run(BackgroundWorkScheduler::ioPool(),
                                         [request]() {
        DocumentEncodingReloadResult result;
        result.requestId = request.requestId;
        result.filePath = request.filePath;
        result.detectedLanguage = int(LexerFactory::Unknown);
        result.success = DocumentIO::loadEditorFileWithEncoding(request.filePath,
                                                                request.encoding,
                                                                &result.content,
                                                                &result.format,
                                                                &result.errorMessage);
        if (result.success && result.content.size() <= PerformanceConstants::LargeFileSize)
        {
            result.performanceDegraded =
                PerformanceConstants::containsAbnormallyLongLine(result.content);
            if (!result.performanceDegraded)
            {
                LexerFactory::Language language =
                    LexerFactory::languageFromFilePath(request.filePath);
                if (language == LexerFactory::Unknown)
                    language = LexerFactory::languageFromContent(result.content);
                result.detectedLanguage = int(language);
            }
        }
        return result;
    }));
}
