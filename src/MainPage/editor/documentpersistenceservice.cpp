#include "editor/documentpersistenceservice.h"

#include "editor/customeditor.h"
#include "editor/documentio.h"
#include "editor/lexerfactory.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>

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
