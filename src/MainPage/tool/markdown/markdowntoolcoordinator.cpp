#include "tool/markdown/markdowntoolcoordinator.h"

#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/editorpane.h"
#include "tool/markdown/markdownpreviewer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

MarkdownToolCoordinator::MarkdownToolCoordinator(QWidget *dialogParent, QObject *parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
    , m_previewer(new MarkdownPreviewer(this))
{
}

bool MarkdownToolCoordinator::isMarkdownFile(const QString &filePath) const
{
    return m_previewer->isMarkdownFile(filePath);
}

void MarkdownToolCoordinator::togglePreview(DocumentController *controller)
{
    if (!controller)
        return;

    CustomEditor *editor = controller->currentEditor();
    if (!editor || !isMarkdownFile(editor->filePath()))
        return;

    const bool visible = controller->pane()->toggleMarkdownPreview(m_previewer);
    controller->pane()->updateSidePreviewButton();
    emit previewStateChanged(editor, visible);
}

void MarkdownToolCoordinator::exportToPdf(CustomEditor *editor)
{
    if (!editor)
        return;

    const QString currentFilePath = editor->filePath();
    if (!isMarkdownFile(currentFilePath))
        return;

    const QFileInfo fileInfo(currentFilePath);
    const QString defaultPath = fileInfo.absolutePath()
                                + QDir::separator()
                                + fileInfo.completeBaseName()
                                + QStringLiteral(".pdf");
    QString outputPath = QFileDialog::getSaveFileName(
        m_dialogParent,
        QCoreApplication::translate("MainWindow", "Export Markdown to PDF"),
        defaultPath,
        QCoreApplication::translate("MainWindow", "PDF Files (*.pdf)"));
    if (outputPath.isEmpty())
        return;

    if (QFileInfo(outputPath).suffix().isEmpty())
        outputPath += QStringLiteral(".pdf");

    if (!m_previewer->exportToPdf(editor->text(), currentFilePath, outputPath))
    {
        QMessageBox::warning(
            m_dialogParent,
            QCoreApplication::translate("MainWindow", "Export Failed"),
            QCoreApplication::translate("MainWindow", "Could not export Markdown to PDF."));
    }
}
