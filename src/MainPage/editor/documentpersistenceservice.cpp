#include "editor/documentpersistenceservice.h"

#include "editor/customeditor.h"
#include "editor/documentio.h"
#include "editor/documentsavecoordinator.h"
#include <NotePeek/lexerfactory.h>
#include "editor/performanceconstants.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QRegularExpression>
#include <QtConcurrent>
#include <Qsci/qsciscintillabase.h>

#include <limits>
#include <cstring>
DocumentPersistenceService::DocumentPersistenceService(DocumentSaveCoordinator *saveCoordinator,
                                                       QObject *parent)
    : QObject(parent)
    , m_saveCoordinator(saveCoordinator)
    , m_loadPool(new QThreadPool(this))
{
    Q_ASSERT(m_saveCoordinator);
    m_loadPool->setMaxThreadCount(2);
}

bool DocumentPersistenceService::saveEditorSnapshot(CustomEditor *editor,
                                                     QByteArray *utf8Content,
                                                     QString *errorMessage) const
{
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
    const DocumentFormat format = editor->documentFormat();
    if ((format.encoding == DocumentEncoding::Utf8
         || format.encoding == DocumentEncoding::Utf8Bom)
        && (format.lineEnding == DocumentLineEnding::Lf
            || format.lineEnding == DocumentLineEnding::Mixed))
    {
        const char *buffer = reinterpret_cast<const char *>(
            editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETCHARACTERPOINTER));
        if (!buffer && length > 0)
        {
            if (errorMessage)
                *errorMessage = tr("Cannot access editor buffer.");
            return false;
        }
        utf8Content->clear();
        if (format.encoding == DocumentEncoding::Utf8Bom)
            utf8Content->append("\xef\xbb\xbf", 3);
        utf8Content->append(buffer, int(length));
    }
    else
    {
        // Convert straight from Scintilla's buffer; text() would copy the
        // whole document an extra time before the encode pass.
        const char *buffer = reinterpret_cast<const char *>(
            editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETCHARACTERPOINTER));
        const QString text = buffer ? QString::fromUtf8(buffer, int(length))
                                    : editor->text();
        *utf8Content = DocumentFormatCodec::encode(text, format);
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

    watcher->setFuture(QtConcurrent::run(m_loadPool, [request]() {
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

void DocumentPersistenceService::saveFileAsync(const DocumentSaveRequest &request)
{
    m_saveCoordinator->enqueue(this, request);
}

void DocumentPersistenceService::waitForPendingSavePath(const QString &filePath)
{
    m_saveCoordinator->waitForPath(filePath);
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
        return QStringLiteral("untitled.txt");
    }

    return QFileInfo(editor->filePath()).fileName();
}
