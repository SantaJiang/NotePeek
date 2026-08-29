#include "editor/documentfiledialogservice.h"

#include <NotePeek/lexerfactory.h>

#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

constexpr auto TextFilesFilter = QT_TRANSLATE_NOOP(
    "DocumentPersistenceService", "Text Files (*.txt)");
constexpr auto AllFilesFilter = QT_TRANSLATE_NOOP(
    "DocumentPersistenceService", "All Files (*)");
constexpr auto SaveAsTitle = QT_TRANSLATE_NOOP(
    "DocumentPersistenceService", "Save As");

QString persistenceTranslation(const char *sourceText)
{
    return QCoreApplication::translate("DocumentPersistenceService", sourceText);
}

} // namespace

QString DocumentFileDialogService::promptSaveFileName(QWidget *parent)
{
    const QString textFiles = persistenceTranslation(TextFilesFilter);
    const QString allFiles = persistenceTranslation(AllFilesFilter);
    QStringList filters;
    filters.append(textFiles);
    for (const QString &extension : LexerFactory::supportedExtensions())
    {
        filters.append(QStringLiteral("%1 (*.%2)").arg(
            LexerFactory::languageName(LexerFactory::languageFromExtension(extension)),
            extension));
    }
    for (const QString &fileName : LexerFactory::supportedFileNames())
    {
        filters.append(QStringLiteral("%1 (%2)").arg(
            LexerFactory::languageName(LexerFactory::languageFromFileName(fileName)),
            fileName));
    }
    filters.append(allFiles);

    QString selectedFilter;
    QString fileName = QFileDialog::getSaveFileName(
        parent,
        persistenceTranslation(SaveAsTitle),
        QStringLiteral("untitled.txt"),
        filters.join(QStringLiteral(";;")),
        &selectedFilter);
    if (fileName.isEmpty())
        return {};

    if (QFileInfo(fileName).suffix().isEmpty()
        && !selectedFilter.isEmpty()
        && selectedFilter != allFiles)
    {
        const QRegularExpression expression(QStringLiteral("\\(\\*\\.([^)]+)\\)"));
        const QRegularExpressionMatch match = expression.match(selectedFilter);
        if (match.hasMatch() && !match.captured(1).isEmpty())
            fileName += u'.' + match.captured(1);
    }
    return fileName;
}

QString DocumentFileDialogService::displayNameForSavePrompt(const QString &filePath)
{
    return filePath.isEmpty() ? QStringLiteral("untitled.txt")
                              : QFileInfo(filePath).fileName();
}
