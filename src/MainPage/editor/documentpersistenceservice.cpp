#include "editor/documentpersistenceservice.h"

#include "editor/customeditor.h"
#include "editor/documentio.h"
#include "editor/lexerfactory.h"
#include "editor/performanceconstants.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QtConcurrent>
#include <Qsci/qsciscintillabase.h>

#include <cstring>
#include <limits>

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
    auto *watcher = new QFutureWatcher<DocumentSaveResult>(this);
    connect(watcher, &QFutureWatcher<DocumentSaveResult>::finished, this, [this, watcher]() {
        const DocumentSaveResult result = watcher->result();
        watcher->deleteLater();
        emit fileSaved(result);
    });

    watcher->setFuture(QtConcurrent::run([request]() {
        DocumentSaveResult result;
        result.requestId = request.requestId;
        result.filePath = request.filePath;
        result.success = DocumentIO::saveUtf8File(request.filePath, request.utf8Content, &result.errorMessage);
        return result;
    }));
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
