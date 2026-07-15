#include "editor/documentpersistenceservice.h"

#include "editor/customeditor.h"
#include "editor/documentio.h"
#include "editor/lexerfactory.h"
#include "editor/performanceconstants.h"

#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHash>
#include <QPointer>
#include <QQueue>
#include <QRegularExpression>
#include <QSet>
#include <QtConcurrent>
#include <Qsci/qsciscintillabase.h>

#include <cstring>
#include <limits>

namespace {

QString normalizedSavePath(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QString canonicalFile = info.canonicalFilePath();
    if (!canonicalFile.isEmpty())
    {
        return QDir::cleanPath(canonicalFile);
    }

    const QString canonicalDirectory = QDir(info.absolutePath()).canonicalPath();
    if (!canonicalDirectory.isEmpty())
    {
        return QDir::cleanPath(QDir(canonicalDirectory).filePath(info.fileName()));
    }
    return QDir::cleanPath(info.absoluteFilePath());
}

class SaveQueueCoordinator : public QObject
{
public:
    struct Job
    {
        QPointer<DocumentPersistenceService> receiver;
        DocumentSaveRequest request;
    };

    void enqueue(DocumentPersistenceService *receiver, const DocumentSaveRequest &request)
    {
        const QString path = normalizedSavePath(request.filePath);
        QQueue<Job> &queue = m_queues[path];
        Job superseded;
        bool hasSupersededJob = false;

        // Keep only the newest queued snapshot for the same shared document.
        // The active write is allowed to finish; replacing queued work bounds
        // rapid-save memory use to one pending snapshot per document/path.
        if (request.documentId != 0)
        {
            for (int i = queue.size() - 1; i >= 0; --i)
            {
                if (queue.at(i).request.documentId != request.documentId)
                {
                    continue;
                }

                superseded = queue.takeAt(i);
                hasSupersededJob = true;
                break;
            }
        }

        queue.enqueue({receiver, request});
        startNext(path);

        // Publish only after the queue mutation is complete. Slots are allowed
        // to enqueue more saves and could otherwise invalidate the hash entry
        // referenced above through a rehash.
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

    void waitForPath(const QString &filePath)
    {
        const QString path = normalizedSavePath(filePath);
        if (!m_activePaths.contains(path) && m_queues.value(path).isEmpty())
        {
            return;
        }

        QEventLoop loop;
        m_waiters[path].append(&loop);
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }

private:
    void startNext(const QString &path)
    {
        if (m_activePaths.contains(path))
        {
            return;
        }

        QQueue<Job> &queue = m_queues[path];
        if (queue.isEmpty())
        {
            m_queues.remove(path);
            const QList<QPointer<QEventLoop>> waiters = m_waiters.take(path);
            for (const QPointer<QEventLoop> &waiter : waiters)
            {
                if (waiter)
                {
                    waiter->quit();
                }
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

            // Start the next write before publishing completion. A completion
            // slot may enter a nested close/save loop, so the FIFO must already
            // be able to make progress inside that loop.
            startNext(path);

            if (job.receiver)
            {
                emit job.receiver->fileSaved(result);
            }
        });
        watcher->setFuture(QtConcurrent::run([request = job.request]() {
            DocumentSaveResult result;
            result.requestId = request.requestId;
            result.filePath = request.filePath;
            result.success = DocumentIO::saveUtf8File(request.filePath,
                                                       request.utf8Content,
                                                       &result.errorMessage);
            return result;
        }));
    }

    QHash<QString, QQueue<Job>> m_queues;
    QSet<QString> m_activePaths;
    QHash<QString, QList<QPointer<QEventLoop>>> m_waiters;
};

SaveQueueCoordinator *saveQueueCoordinator()
{
    // Process lifetime is intentional: queued snapshots must survive the
    // controller or window that submitted them.
    static SaveQueueCoordinator *coordinator = new SaveQueueCoordinator;
    return coordinator;
}

} // namespace

DocumentPersistenceService::DocumentPersistenceService(QObject *parent)
    : QObject(parent)
{
}

bool DocumentPersistenceService::loadFile(const QString &filePath, QString *content, QString *errorMessage) const
{
    return DocumentIO::loadTextFile(filePath, content, errorMessage);
}

bool DocumentPersistenceService::saveEditor(CustomEditor *editor, const QString &filePath, QString *errorMessage) const
{
    if (!editor)
    {
        return false;
    }

    return DocumentIO::saveTextFile(filePath, editor->text(), errorMessage);
}

bool DocumentPersistenceService::saveEditorSnapshot(CustomEditor *editor,
                                                    const QString &filePath,
                                                    QByteArray *utf8Content,
                                                    QString *errorMessage) const
{
    Q_UNUSED(filePath);
    if (!editor || !utf8Content)
    {
        if (errorMessage)
        {
            *errorMessage = tr("No editor content to save.");
        }
        return false;
    }

    const auto length = editor->SendScintilla(QsciScintillaBase::SCI_GETLENGTH);
    if (length > std::numeric_limits<int>::max())
    {
        if (errorMessage)
        {
            *errorMessage = tr("The document is too large to snapshot for saving.");
        }
        return false;
    }
    const char *buffer = reinterpret_cast<const char *>(
        editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETCHARACTERPOINTER));
    if (!buffer && length > 0)
    {
        if (errorMessage)
        {
            *errorMessage = tr("Cannot access editor buffer.");
        }
        return false;
    }

    utf8Content->clear();
    utf8Content->resize(static_cast<int>(length));
    if (length > 0)
    {
        memcpy(utf8Content->data(), buffer, static_cast<size_t>(length));
    }
    return true;
}

void DocumentPersistenceService::loadFileAsync(const DocumentLoadRequest &request)
{
    auto *watcher = new QFutureWatcher<DocumentLoadResult>(this);
    connect(watcher, &QFutureWatcher<DocumentLoadResult>::finished, this, [this, watcher]() {
        const DocumentLoadResult result = watcher->result();
        watcher->deleteLater();
        emit fileLoaded(result);
    });

    watcher->setFuture(QtConcurrent::run([request]() {
        DocumentLoadResult result;
        result.requestId = request.requestId;
        result.filePath = request.filePath;
        result.fileSize = QFileInfo(request.filePath).size();
        result.largeFile = result.fileSize > PerformanceConstants::LargeFileSize;
        result.success = DocumentIO::loadTextFile(request.filePath, &result.content, &result.errorMessage);
        return result;
    }));
}

void DocumentPersistenceService::saveFileAsync(const DocumentSaveRequest &request)
{
    saveQueueCoordinator()->enqueue(this, request);
}

void DocumentPersistenceService::waitForPendingSavePath(const QString &filePath)
{
    saveQueueCoordinator()->waitForPath(filePath);
}

QString DocumentPersistenceService::promptSaveFileName(QWidget *parent) const
{
    QStringList filters;
    filters.append(tr("Text Files (*.txt)"));
    for (const QString &ext : LexerFactory::supportedExtensions())
    {
        filters.append(
            QStringLiteral("%1 (*.%2)").arg(LexerFactory::languageName(LexerFactory::languageFromExtension(ext)), ext));
    }
    for (const QString &fileName : LexerFactory::supportedFileNames())
    {
        filters.append(QStringLiteral("%1 (%2)").arg(
            LexerFactory::languageName(LexerFactory::languageFromFileName(fileName)), fileName));
    }
    filters.append(tr("All Files (*)"));

    QString selectedFilter;
    QString fileName = QFileDialog::getSaveFileName(parent, tr("Save As"), QStringLiteral("untitled.txt"),
                                                    filters.join(QStringLiteral(";;")), &selectedFilter);
    if (fileName.isEmpty())
    {
        return {};
    }

    if (QFileInfo(fileName).suffix().isEmpty() && !selectedFilter.isEmpty() && selectedFilter != tr("All Files (*)"))
    {
        const QRegularExpression re(QStringLiteral("\\(\\*\\.([^)]+)\\)"));
        const QRegularExpressionMatch match = re.match(selectedFilter);
        if (match.hasMatch())
        {
            const QString ext = match.captured(1);
            if (!ext.isEmpty())
            {
                fileName += u'.' + ext;
            }
        }
    }
    return fileName;
}

QString DocumentPersistenceService::displayNameForSavePrompt(CustomEditor *editor) const
{
    if (!editor || editor->filePath().isEmpty())
    {
        return tr("untitled.txt");
    }

    return QFileInfo(editor->filePath()).fileName();
}
