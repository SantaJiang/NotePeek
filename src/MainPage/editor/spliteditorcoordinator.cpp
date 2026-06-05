#include "spliteditorcoordinator.h"

#include "customeditor.h"
#include "multieditor.h"

#include <QScopedValueRollback>
#include <QSplitter>
#include <Qsci/qsciscintillabase.h>

SplitEditorCoordinator::SplitEditorCoordinator(MultiEditor *leftEditor,
                                               MultiEditor *rightEditor,
                                               QSplitter *splitter,
                                               bool singleFileWindow,
                                               QObject *parent)
    : QObject(parent)
    , m_leftEditor(leftEditor)
    , m_rightEditor(rightEditor)
    , m_splitter(splitter)
    , m_activeMultiEditor(leftEditor)
    , m_syncingEditorPanes(false)
    , m_singleFileWindow(singleFileWindow)
{
    connectMultiEditor(m_leftEditor);
    connectMultiEditor(m_rightEditor);
    setSplitViewEnabled(false);
}

MultiEditor *SplitEditorCoordinator::activeMultiEditor() const
{
    return m_activeMultiEditor ? m_activeMultiEditor : m_leftEditor;
}

QList<MultiEditor *> SplitEditorCoordinator::editorPanes() const
{
    return { m_leftEditor, m_rightEditor };
}

void SplitEditorCoordinator::setActiveMultiEditor(MultiEditor *multiEditor)
{
    if (!multiEditor)
        return;

    m_activeMultiEditor = multiEditor;
    emit activeMultiEditorChanged(multiEditor);
}

void SplitEditorCoordinator::setSplitViewEnabled(bool enabled)
{
    if (m_singleFileWindow)
    {
        enabled = false;
    }

    if (!enabled && activeMultiEditor() == m_rightEditor)
    {
        setActiveMultiEditor(m_leftEditor);
    }

    m_rightEditor->setVisible(enabled);
    m_leftEditor->setSplitViewButtonChecked(enabled);
    m_rightEditor->setSplitViewButtonChecked(enabled);

    if (enabled)
    {
        const QList<int> sizes = m_splitter->sizes();
        if (sizes.size() < 2 || sizes.at(1) == 0)
        {
            m_splitter->setSizes({ 1, 1 });
        }
        showSameFileInRightPane();
    }
}

bool SplitEditorCoordinator::closeAllEditors()
{
    for (MultiEditor *multiEditor : editorPanes())
    {
        setActiveMultiEditor(multiEditor);
        if (!multiEditor->closeAllEditors())
        {
            return false;
        }
    }
    return true;
}

void SplitEditorCoordinator::reapplyThemeToEditorPanes()
{
    for (MultiEditor *multiEditor : editorPanes())
    {
        multiEditor->editorManager()->reapplyThemeToAllEditors();
    }
}

void SplitEditorCoordinator::refreshVisibleMarkdownPreviews()
{
    for (MultiEditor *multiEditor : editorPanes())
    {
        multiEditor->refreshVisibleMarkdownPreview();
    }
}

void SplitEditorCoordinator::updateActivePaneForFocus(QWidget *widget)
{
    if (!widget)
        return;

    for (MultiEditor *multiEditor : editorPanes())
    {
        if (multiEditor == widget || multiEditor->isAncestorOf(widget))
        {
            setActiveMultiEditor(multiEditor);
            break;
        }
    }
}

void SplitEditorCoordinator::connectMultiEditor(MultiEditor *multiEditor)
{
    if (!multiEditor)
        return;

    connect(multiEditor, &MultiEditor::currentEditorChanged, this, [this, multiEditor](CustomEditor *) {
        if (!m_syncingEditorPanes)
        {
            setActiveMultiEditor(multiEditor);
        }
    });
    connect(multiEditor, &MultiEditor::modificationChanged, this, [this, multiEditor](bool modified) {
        emit paneModificationChanged(multiEditor, modified);
    });
    connect(multiEditor, &MultiEditor::currentFileChanged, this, [this, multiEditor](CustomEditor *, const QString &filePath) {
        if (!m_syncingEditorPanes)
        {
            setActiveMultiEditor(multiEditor);
        }
        syncOpenedFileToOtherPanes(multiEditor, filePath);
    });
    connect(multiEditor, &MultiEditor::fileOpened, this, [this, multiEditor](CustomEditor *, const QString &filePath) {
        syncOpenedFileToOtherPanes(multiEditor, filePath);
    });
    connect(multiEditor, &MultiEditor::filePathChanged, this, [this, multiEditor](const QString &oldFilePath, const QString &newFilePath) {
        syncRenamedFileToOtherPanes(multiEditor, oldFilePath, newFilePath);
    });
    connect(multiEditor, &MultiEditor::editorClosed, this, [this, multiEditor](CustomEditor *editor) {
        syncClosedFileToOtherPanes(multiEditor, editor ? editor->filePath() : QString());
    });
    connect(multiEditor, &MultiEditor::copyAvailable, this, [this, multiEditor](bool available) {
        emit paneCopyAvailableChanged(multiEditor, available);
    });
    connect(multiEditor, &MultiEditor::filesDropped, this, [this, multiEditor](const QList<QUrl> &urls) {
        setActiveMultiEditor(multiEditor);
        emit filesDropped(multiEditor, urls);
    });
    connect(multiEditor, &MultiEditor::splitViewToggled, this, [this, multiEditor](bool checked) {
        setActiveMultiEditor(multiEditor);
        setSplitViewEnabled(checked);
    });
}

void SplitEditorCoordinator::syncOpenedFileToOtherPanes(MultiEditor *source, const QString &filePath)
{
    if (m_syncingEditorPanes || filePath.isEmpty())
        return;

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    for (MultiEditor *multiEditor : editorPanes())
    {
        if (multiEditor != source && multiEditor->findFileIndex(filePath) == -1)
        {
            multiEditor->openFile(filePath, false);
        }
    }
}

void SplitEditorCoordinator::syncClosedFileToOtherPanes(MultiEditor *source, const QString &filePath)
{
    if (m_syncingEditorPanes || filePath.isEmpty())
        return;

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    for (MultiEditor *multiEditor : editorPanes())
    {
        if (multiEditor == source)
            continue;

        const int index = multiEditor->findFileIndex(filePath);
        if (index != -1)
        {
            multiEditor->tabCloseRequested(index);
        }
    }
}

void SplitEditorCoordinator::syncRenamedFileToOtherPanes(MultiEditor *source,
                                                         const QString &oldFilePath,
                                                         const QString &newFilePath)
{
    if (m_syncingEditorPanes || newFilePath.isEmpty())
        return;

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    for (MultiEditor *multiEditor : editorPanes())
    {
        if (multiEditor == source)
            continue;

        if (multiEditor->findFileIndex(newFilePath) == -1)
        {
            multiEditor->openFile(newFilePath, false);
        }
        if (!oldFilePath.isEmpty() && oldFilePath != newFilePath)
        {
            multiEditor->closeFileIfUnmodified(oldFilePath);
        }
    }
}

void SplitEditorCoordinator::showSameFileInRightPane()
{
    CustomEditor *leftEditor = m_leftEditor->currentEditor();
    if (!leftEditor)
        return;

    const QString currentFilePath = leftEditor->filePath();
    if (currentFilePath.isEmpty())
        return;

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    for (int i = 0; i < m_leftEditor->count(); ++i)
    {
        const QString filePath = m_leftEditor->filePathAt(i);
        if (!filePath.isEmpty() && m_rightEditor->findFileIndex(filePath) == -1)
        {
            m_rightEditor->openFile(filePath, false);
        }
    }

    int line = 0;
    int index = 0;
    leftEditor->getCursorPosition(&line, &index);
    const int firstVisibleLine = leftEditor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
    const int xOffset = leftEditor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET);

    m_rightEditor->openFile(currentFilePath, true);

    CustomEditor *rightEditor = m_rightEditor->currentEditor();
    if (rightEditor)
    {
        const int targetLine = qMin(line, rightEditor->lines() - 1);
        const int targetIndex = qMin(index, rightEditor->lineLength(targetLine));
        rightEditor->setCursorPosition(targetLine, targetIndex);
        rightEditor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
        rightEditor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);
    }
    setActiveMultiEditor(m_leftEditor);
}
