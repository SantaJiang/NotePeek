#include "editor/documentcontroller.h"

#include "apppreferences.h"
#include "decorators/highlightedscrollbardecorator.h"
#include "editor/customeditor.h"
#include "editor/documentpage.h"
#include "editor/documentpersistenceservice.h"
#include "editor/documentregistry.h"
#include "editor/editormanager.h"
#include "editor/externalfilewatcher.h"
#include "editor/filesavedialog.h"
#include "editor/lexerfactory.h"
#include "editor/editorpane.h"
#include "find/findreplacemanager.h"

#include <QApplication>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QSet>
#include <QSignalBlocker>

namespace {

bool isMarkdownPath(const QString &filePath)
{
    return LexerFactory::languageFromFilePath(filePath) == LexerFactory::MARKDOWN;
}

} // namespace

DocumentController::DocumentController(EditorPane *pane,
                                       bool createDefaultWhenEmpty,
                                       QObject *parent)
    : QObject(parent)
    , m_pane(pane)
    , m_editorManager(new EditorManager(this))
    , m_persistence(new DocumentPersistenceService(this))
    , m_externalFileWatcher(new ExternalFileWatcher(this))
    , m_defaultEditor(nullptr)
    , m_createDefaultWhenEmpty(createDefaultWhenEmpty)
{
    connect(m_editorManager, &EditorManager::filesDropped,
            this, &DocumentController::filesDropped);
    connect(m_externalFileWatcher, &ExternalFileWatcher::externalFileChanged,
            this, &DocumentController::externalFileChanged);
    connect(m_pane, &EditorPane::pageCloseRequested,
            this, &DocumentController::requestClose);
    connect(m_pane, &EditorPane::currentPageChanged,
            this, &DocumentController::onCurrentPageChanged);

    if (m_createDefaultWhenEmpty)
    {
        createDefaultDocument();
    }
}

CustomEditor *DocumentController::currentEditor() const
{
    return m_pane->currentEditor();
}

void DocumentController::setCreateDefaultWhenEmpty(bool createDefault)
{
    m_createDefaultWhenEmpty = createDefault;

    if (!createDefault && m_defaultEditor)
    {
        closeDefaultEditorIfUnused();
    }
    else if (createDefault && !m_defaultEditor && m_pane->count() == 0)
    {
        createDefaultDocument();
    }
}

void DocumentController::attachFindReplaceManager(FindReplaceManager *manager)
{
    CustomEditor *editor = currentEditor();
    if (!editor || !manager)
        return;

    manager->setCurrentEditor(editor);

    auto *decorator = editor->findChild<HighlightedScrollBarDecorator *>();
    if (decorator)
    {
        connect(manager, &FindReplaceManager::searchResultsChanged,
                decorator, &HighlightedScrollBarDecorator::updateScrollBar,
                Qt::UniqueConnection);
    }
}

void DocumentController::openFile(const QString &filePath, bool activate)
{
    const int previousIndex = m_pane->currentIndex();

    // If the file is already open, just activate it.
    CustomEditor *existingEditor = m_editorManager->getEditorByFilePath(filePath);
    if (existingEditor)
    {
        closeDefaultEditorIfUnused();
        const int index = m_pane->indexOfEditor(existingEditor);
        if (index != -1 && activate)
        {
            m_pane->setCurrentIndex(index);
        }
        return;
    }

    CustomEditor *editor = m_editorManager->createEditorForFile(filePath, false);
    if (!editor)
    {
        return;
    }

    const QFileInfo fileInfo(filePath);
    DocumentPage *page = createPage(editor, filePath);
    const int tabIndex = m_pane->addPage(page, fileInfo.fileName(), filePath);
    m_pane->setCurrentIndex(tabIndex);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString content;
    QString errorMessage;
    const bool loaded = m_persistence->loadFile(filePath, &content, &errorMessage);
    QApplication::restoreOverrideCursor();

    if (!loaded)
    {
        m_pane->removePage(tabIndex);
        emit editorClosed(editor);
        m_editorManager->forgetEditor(editor);
        page->deleteLater();

        if (m_createDefaultWhenEmpty && m_pane->count() == 0)
        {
            createDefaultDocument();
        }

        QMessageBox::warning(m_pane, tr("NotePeek"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(filePath)
                             .arg(errorMessage));
        return;
    }

    editor->setText(content);

    // Honour the "open Markdown in preview" preference for freshly opened files.
    if (AppPreferences::instance().markdownDefaultPreview() && m_markdownPreviewer)
    {
        if (auto *markdownPage = qobject_cast<MarkdownDocumentPage *>(page))
        {
            // Background opens must not steal focus from the file being
            // activated elsewhere (e.g. the split-view mirror).
            markdownPage->setInitialViewMode(true, m_markdownPreviewer, activate);
        }
    }

    setCurrentFile(editor, filePath);
    m_editorManager->updateLineNumberMarginWidth(editor);
    closeDefaultEditorIfUnused();
    if (activate)
    {
        m_pane->focusEditor(editor);
    }
    else if (previousIndex >= 0 && previousIndex < m_pane->count())
    {
        m_pane->setCurrentIndex(previousIndex);
    }

    emit fileOpened(editor, filePath);

    connectEditorSignals(editor);
}

void DocumentController::newDocument()
{
    if (maybeSave(currentEditor()))
    {
        createDefaultDocument();
    }
}

bool DocumentController::save()
{
    CustomEditor *editor = currentEditor();
    if (!editor)
        return false;

    const QString filePath = editor->filePath();
    return filePath.isEmpty() ? saveAs() : saveFile(editor, filePath);
}

bool DocumentController::saveAs()
{
    CustomEditor *editor = currentEditor();
    if (!editor)
        return false;

    const QString fileName = m_persistence->promptSaveFileName(m_pane);
    if (fileName.isEmpty())
        return false;

    return saveFile(editor, fileName);
}

bool DocumentController::maybeSave(CustomEditor *editor)
{
    if (!editor || !editor->isModified())
        return true;

    m_pane->focusEditor(editor);

    QMap<CustomEditor *, QString> files;
    files.insert(editor, m_persistence->displayNameForSavePrompt(editor));

    FileSaveDialog dialog(files, m_pane);
    const int result = dialog.exec();

    if (result == QDialog::Accepted)
    {
        const QString filePath = editor->filePath();
        return filePath.isEmpty() ? saveAs() : saveFile(editor, filePath);
    }
    if (result == FileSaveDialog::DontSave)
    {
        return true;
    }

    return false;
}

bool DocumentController::saveEditorForClose(CustomEditor *editor)
{
    if (!editor)
    {
        return false;
    }

    QString filePath = editor->filePath();
    if (filePath.isEmpty())
    {
        filePath = m_persistence->promptSaveFileName(m_pane->parentWidget());
        if (filePath.isEmpty())
        {
            return false;
        }
    }

    return saveFile(editor, filePath);
}

bool DocumentController::saveFile(CustomEditor *editor, const QString &filePath)
{
    if (!editor)
        return false;

    const QString oldFilePath = editor->filePath().isEmpty()
                                    ? QString()
                                    : QFileInfo(editor->filePath()).absoluteFilePath();
    const QString newFilePath = QFileInfo(filePath).absoluteFilePath();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString errorMessage;
    const bool saved = m_persistence->saveEditor(editor, filePath, &errorMessage);
    QApplication::restoreOverrideCursor();

    if (!saved)
    {
        QMessageBox::warning(m_pane, tr("NotePeek"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(filePath)
                             .arg(errorMessage));
        return false;
    }

    recordFileState(newFilePath);
    setCurrentFile(editor, filePath);
    m_editorManager->updateEditorSyntax(editor, filePath);
    if (newFilePath != oldFilePath)
    {
        refreshPageTypeForFile(editor, filePath);
        emit filePathChanged(editor, oldFilePath, newFilePath);
    }
    return true;
}

void DocumentController::requestClose(int index)
{
    DocumentPage *page = m_pane->pageAt(index);
    CustomEditor *editor = page ? page->editor() : nullptr;
    if (!editor)
        return;

    if (editor == m_defaultEditor)
    {
        m_defaultEditor = nullptr;
    }

    if (maybeSave(editor))
    {
        m_pane->removePage(index);
        emit editorClosed(editor);

        m_editorManager->forgetEditor(editor);
        page->deleteLater();
        refreshWatchedFiles();

        if (m_createDefaultWhenEmpty && m_pane->count() == 0)
        {
            createDefaultDocument();
        }
    }
}

bool DocumentController::closeFileIfUnmodified(const QString &filePath)
{
    const int index = indexOfFile(filePath);
    if (index == -1)
    {
        return true;
    }

    DocumentPage *page = m_pane->pageAt(index);
    CustomEditor *editor = page ? page->editor() : nullptr;
    if (!editor || editor->isModified())
    {
        return false;
    }

    if (editor == m_defaultEditor)
    {
        m_defaultEditor = nullptr;
    }

    m_pane->removePage(index);
    emit editorClosed(editor);
    m_editorManager->forgetEditor(editor);
    page->deleteLater();
    refreshWatchedFiles();

    if (m_createDefaultWhenEmpty && m_pane->count() == 0)
    {
        createDefaultDocument();
    }

    return true;
}

void DocumentController::createDefaultDocument()
{
    CustomEditor *editor = m_editorManager->createEditor(tr("Untitled"));
    DocumentPage *page = createPage(editor, QString());
    const int tabIndex = m_pane->addPage(page, tr("Untitled"), QString());
    m_pane->setCurrentIndex(tabIndex);

    m_defaultEditor = editor;

    connectEditorSignals(editor);

    emit editorCreated(editor);
}

int DocumentController::indexOfFile(const QString &filePath) const
{
    CustomEditor *editor = m_editorManager->getEditorByFilePath(filePath);
    return editor ? m_pane->indexOfEditor(editor) : -1;
}

void DocumentController::openSharedDocument(CustomEditor *sourceEditor, bool activate)
{
    if (!sourceEditor)
    {
        return;
    }

    const int existingIndex = findSharedDocumentIndex(sourceEditor);
    if (existingIndex != -1)
    {
        if (activate)
        {
            m_pane->setCurrentIndex(existingIndex);
        }
        return;
    }

    const QString filePath = sourceEditor->filePath();
    QString displayName = filePath.isEmpty() ? tr("Untitled") : QFileInfo(filePath).fileName();
    if (sourceEditor->isModified())
    {
        displayName += '*';
    }

    CustomEditor *editor = m_editorManager->createEditorSharingDocument(sourceEditor, displayName);
    if (!editor)
    {
        return;
    }

    DocumentPage *page = createPage(editor, filePath);
    connectEditorSignals(editor);
    const int tabIndex = m_pane->addPage(page, displayName, filePath);

    if (filePath.isEmpty() && !sourceEditor->isModified() && sourceEditor->text().isEmpty())
    {
        m_defaultEditor = editor;
    }

    if (activate)
    {
        m_pane->setCurrentIndex(tabIndex);
    }

    emit editorCreated(editor);
    emit currentEditorChanged(editor);
    emit modificationChanged(editor->isModified());
    recordFileState(filePath);
    refreshWatchedFiles();
}

bool DocumentController::adoptSharedDocumentPath(CustomEditor *sourceEditor, const QString &newFilePath)
{
    if (!sourceEditor || newFilePath.isEmpty())
    {
        return false;
    }

    const int index = findSharedDocumentIndex(sourceEditor);
    if (index == -1)
    {
        return false;
    }

    CustomEditor *editor = m_pane->editorAt(index);
    if (!editor)
    {
        return false;
    }

    setCurrentFile(editor, newFilePath);
    m_editorManager->updateEditorSyntax(editor, newFilePath);
    refreshPageTypeForFile(editor, newFilePath);
    return true;
}

bool DocumentController::reloadFileFromDiskIfUnmodified(const QString &filePath)
{
    const QString normalizedPath = DocumentRegistry::normalizedFilePath(filePath);
    if (normalizedPath.isEmpty()
        || !DocumentRegistry::trackedFilePaths(m_editorManager->getEditors()).contains(normalizedPath))
    {
        return false;
    }

    const QFileInfo fileInfo(normalizedPath);
    const bool exists = fileInfo.exists() && fileInfo.isFile();

    if (!exists)
    {
        recordFileState(normalizedPath);
        refreshWatchedFiles();
        return false;
    }

    if (hasModifiedEditorForFile(normalizedPath))
    {
        return false;
    }

    QString content;
    QString errorMessage;
    if (!m_persistence->loadFile(normalizedPath, &content, &errorMessage))
    {
        refreshWatchedFiles();
        return false;
    }

    recordFileState(normalizedPath);

    bool reloaded = false;
    bool reloadedCurrentEditor = false;
    QSet<void *> reloadedDocuments;
    for (CustomEditor *editor : m_editorManager->getEditors())
    {
        if (!editor || DocumentRegistry::normalizedFilePath(editor->filePath()) != normalizedPath)
        {
            continue;
        }

        // Editors in split view can share one Scintilla document; reload it once.
        void *document = editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
        if (document && reloadedDocuments.contains(document))
        {
            continue;
        }
        if (document)
        {
            reloadedDocuments.insert(document);
        }

        int line = 0;
        int index = 0;
        editor->getCursorPosition(&line, &index);
        const int firstVisibleLine = editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
        const int xOffset = editor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET);

        const QSignalBlocker blocker(editor);
        editor->setText(content);
        editor->setModified(false);

        const int targetLine = qBound(0, line, qMax(0, editor->lines() - 1));
        const int targetIndex = qBound(0, index, editor->lineLength(targetLine));
        editor->setCursorPosition(targetLine, targetIndex);
        editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
        editor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);
        m_editorManager->updateLineNumberMarginWidth(editor);

        if (DocumentPage *page = m_pane->pageForEditor(editor))
        {
            page->refreshPreview();
        }

        emit editorModificationChanged(editor, false);
        if (editor == currentEditor())
        {
            reloadedCurrentEditor = true;
        }
        reloaded = true;
    }

    if (reloadedCurrentEditor)
    {
        emit modificationChanged(false);
    }

    refreshWatchedFiles();
    return reloaded;
}

bool DocumentController::hasModifiedEditorForFile(const QString &filePath) const
{
    return DocumentRegistry::hasModifiedEditorForFile(m_editorManager->getEditors(), filePath);
}

void DocumentController::onCurrentPageChanged(int index)
{
    if (index == -1)
        return;

    DocumentPage *page = m_pane->pageAt(index);
    CustomEditor *editor = page ? page->editor() : nullptr;
    if (!editor)
        return;

    emit currentEditorChanged(editor);

    // The trailing '*' on the page title marks unsaved changes.
    emit modificationChanged(m_pane->pageTitle(index).endsWith('*'));

    emit copyAvailable(page->hasCopySelection());
}

void DocumentController::onEditorModificationChanged()
{
    CustomEditor *editor = qobject_cast<CustomEditor *>(sender());
    if (!editor)
        return;

    // A default document that has been edited is no longer disposable.
    if (editor == m_defaultEditor && editor->isModified())
    {
        m_defaultEditor = nullptr;
    }

    const int index = m_pane->indexOfEditor(editor);
    if (index == -1)
        return;

    QString title = m_pane->pageTitle(index);
    if (title.endsWith('*'))
    {
        title.chop(1);
    }

    if (editor->isModified())
    {
        m_pane->setPageTitle(index, title + '*');
        emit modificationChanged(true);
    }
    else
    {
        m_pane->setPageTitle(index, title);
        emit modificationChanged(false);
    }

    emit editorModificationChanged(editor, editor->isModified());

    refreshWatchedFiles();
    if (!editor->isModified() && !editor->filePath().isEmpty())
    {
        m_externalFileWatcher->requestExternalChangeCheck(editor->filePath());
    }
}

void DocumentController::connectEditorSignals(CustomEditor *editor)
{
    if (!editor)
        return;

    connect(editor, SIGNAL(modificationChanged(bool)), this, SLOT(onEditorModificationChanged()));
    connect(editor, SIGNAL(copyAvailable(bool)), this, SIGNAL(copyAvailable(bool)));
}

bool DocumentController::closeDefaultEditorIfUnused()
{
    if (!m_defaultEditor || m_defaultEditor->isModified() || !m_defaultEditor->text().isEmpty())
    {
        return false;
    }

    const int index = m_pane->indexOfEditor(m_defaultEditor);
    if (index == -1)
    {
        m_defaultEditor = nullptr;
        return false;
    }

    CustomEditor *editor = m_defaultEditor;
    DocumentPage *page = m_pane->pageForEditor(editor);
    m_pane->removePage(index);
    emit editorClosed(editor);
    m_editorManager->forgetEditor(editor);

    if (page)
    {
        page->deleteLater();
    }
    else
    {
        editor->deleteLater();
    }

    m_defaultEditor = nullptr;
    refreshWatchedFiles();
    return true;
}

DocumentPage *DocumentController::createPage(CustomEditor *editor, const QString &filePath)
{
    DocumentPage *page = isMarkdownPath(filePath)
                             ? static_cast<DocumentPage *>(new MarkdownDocumentPage(editor, m_pane))
                             : static_cast<DocumentPage *>(new PlainDocumentPage(editor, m_pane));
    connect(page, &DocumentPage::copyAvailableChanged, this, &DocumentController::copyAvailable);
    return page;
}

void DocumentController::refreshPageTypeForFile(CustomEditor *editor, const QString &filePath)
{
    DocumentPage *oldPage = m_pane->pageForEditor(editor);
    if (!oldPage)
    {
        return;
    }

    const bool needsMarkdownPage = isMarkdownPath(filePath);
    const bool hasMarkdownPage = qobject_cast<MarkdownDocumentPage *>(oldPage) != nullptr;
    if (needsMarkdownPage == hasMarkdownPage)
    {
        oldPage->hidePreview();
        return;
    }

    const int index = m_pane->indexOfEditor(editor);
    if (index == -1)
    {
        return;
    }

    CustomEditor *ownedEditor = oldPage->takeEditor();
    if (!ownedEditor)
    {
        return;
    }

    DocumentPage *newPage = createPage(ownedEditor, filePath);
    m_pane->replacePage(index, newPage);
    oldPage->deleteLater();

    emit currentEditorChanged(ownedEditor);
    emit copyAvailable(newPage->hasCopySelection());
}

void DocumentController::setCurrentFile(CustomEditor *editor, const QString &filePath)
{
    if (!editor)
        return;

    editor->setFilePath(filePath);
    editor->setModified(false);

    const QString shownName = filePath.isEmpty()
                                  ? QStringLiteral("untitled.txt")
                                  : QFileInfo(filePath).fileName();

    const int index = m_pane->indexOfEditor(editor);
    if (index != -1)
    {
        m_pane->setPageTitle(index, shownName);
        m_pane->setPageToolTip(index, filePath);
    }

    emit editorModificationChanged(editor, false);
    if (editor == currentEditor())
    {
        emit modificationChanged(false);
        emit currentFileChanged(editor, filePath);
    }

    recordFileState(filePath);
    refreshWatchedFiles();
}

int DocumentController::findSharedDocumentIndex(CustomEditor *sourceEditor) const
{
    if (!sourceEditor)
    {
        return -1;
    }

    void *sourceDocument = sourceEditor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
    if (!sourceDocument)
    {
        return -1;
    }

    for (int i = 0; i < m_pane->count(); ++i)
    {
        CustomEditor *editor = m_pane->editorAt(i);
        if (editor && editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER) == sourceDocument)
        {
            return i;
        }
    }

    return -1;
}

void DocumentController::refreshWatchedFiles()
{
    m_externalFileWatcher->setTrackedFilePaths(
        DocumentRegistry::trackedFilePaths(m_editorManager->getEditors()));
}

void DocumentController::recordFileState(const QString &filePath)
{
    m_externalFileWatcher->recordFileState(filePath);
}
