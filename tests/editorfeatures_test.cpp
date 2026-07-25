#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/documentpath.h"
#include "editor/documentsavecoordinator.h"
#include "editor/editorpane.h"
#include "editor/editormanager.h"
#include "editor/externalfilewatcher.h"
#include "editor/spliteditorcoordinator.h"
#include "find/findreplacemanager.h"
#include "tool/markdown/markdownpreviewer.h"
#include "apppreferences.h"

#include <QFile>
#include <QTemporaryDir>
#include <QSplitter>
#include <QtTest>

class EditorFeaturesTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        AppPreferences &prefs = AppPreferences::instance();
        prefs.setCodeFoldingEnabled(true);
        prefs.setBracketAssistanceEnabled(true);
        prefs.setSmartIndentationEnabled(true);
        prefs.setDocumentCompletionEnabled(true);
    }

    void cleanup()
    {
        AppPreferences &prefs = AppPreferences::instance();
        prefs.setCodeFoldingEnabled(false);
        prefs.setBracketAssistanceEnabled(false);
        prefs.setSmartIndentationEnabled(true);
        prefs.setDocumentCompletionEnabled(false);
    }

    void autoClosesAndSkipsBrackets()
    {
        CustomEditor editor;
        editor.setCodeEditingFeaturesEnabled(true);
        editor.setBracketAssistanceEnabled(true);

        QTest::keyClicks(&editor, QStringLiteral("("));
        QCOMPARE(editor.text(), QStringLiteral("()"));

        int line = -1;
        int index = -1;
        editor.getCursorPosition(&line, &index);
        QCOMPARE(line, 0);
        QCOMPARE(index, 1);

        QTest::keyClicks(&editor, QStringLiteral(")"));
        QCOMPARE(editor.text(), QStringLiteral("()"));
        editor.getCursorPosition(&line, &index);
        QCOMPARE(index, 2);

        editor.undo();
        QCOMPARE(editor.text(), QString());
    }

    void wrapsSelectionInBrackets()
    {
        CustomEditor editor;
        editor.setCodeEditingFeaturesEnabled(true);
        editor.setBracketAssistanceEnabled(true);
        editor.setText(QStringLiteral("值abc"));
        editor.setSelection(0, 0, 0, 4);

        QTest::keyClicks(&editor, QStringLiteral("{"));

        QCOMPARE(editor.text(), QStringLiteral("{值abc}"));
        QCOMPARE(editor.selectedText(), QStringLiteral("值abc"));
    }

    void configuresFeaturesForCodeOnly()
    {
        EditorManager manager;
        CustomEditor *editor = manager.createEditor();

        manager.updateEditorSyntax(editor, QStringLiteral("example.cpp"));
        QVERIFY(editor->codeEditingFeaturesEnabled());
        QCOMPARE(editor->margins(), 2);
        QCOMPARE(editor->folding(), CustomEditor::BoxedTreeFoldStyle);
        QVERIFY(editor->marginWidth(1) > 0);
        QVERIFY(editor->indentationGuides());
        QVERIFY(editor->tabIndents());
        QVERIFY(editor->backspaceUnindents());
        QCOMPARE(editor->autoCompletionSource(), CustomEditor::AcsDocument);
        QCOMPARE(editor->autoCompletionThreshold(), 3);

        manager.updateEditorSyntax(editor, QStringLiteral("Dockerfile"));
        QVERIFY(editor->codeEditingFeaturesEnabled());
        QCOMPARE(editor->folding(), CustomEditor::NoFoldStyle);
        QCOMPARE(editor->marginWidth(1), 0);
        QCOMPARE(editor->autoCompletionSource(), CustomEditor::AcsDocument);

        manager.updateEditorSyntax(editor, QStringLiteral("README.md"));
        QVERIFY(!editor->codeEditingFeaturesEnabled());
        QCOMPARE(editor->folding(), CustomEditor::NoFoldStyle);
        QCOMPARE(editor->marginWidth(1), 0);
        QVERIFY(!editor->indentationGuides());
        QCOMPARE(editor->autoCompletionSource(), CustomEditor::AcsNone);

        editor->setText(QString());
        QTest::keyClicks(editor, QStringLiteral("("));
        QCOMPARE(editor->text(), QStringLiteral("("));
    }

    void movesToMatchingBracket()
    {
        CustomEditor editor;
        editor.setBracketAssistanceEnabled(true);
        editor.setText(QStringLiteral("(value)"));
        editor.setCursorPosition(0, 0);

        editor.goToMatchingBrace();

        int line = -1;
        int index = -1;
        editor.getCursorPosition(&line, &index);
        QCOMPARE(line, 0);
        QCOMPARE(index, 7);
    }

    void completesWordsFromCurrentDocument()
    {
        EditorManager manager;
        CustomEditor *editor = manager.createEditor();
        manager.updateEditorSyntax(editor, QStringLiteral("example.cpp"));
        editor->setText(QStringLiteral("alpha alphabet\nalp"));
        editor->setCursorPosition(1, 3);

        editor->completeWordFromDocument();

        QVERIFY(editor->isListActive());
        editor->cancelList();
    }

    void appliesDisabledFeaturePreferencesLive()
    {
        EditorManager manager;
        CustomEditor *editor = manager.createEditor();
        manager.updateEditorSyntax(editor, QStringLiteral("example.cpp"));

        AppPreferences &prefs = AppPreferences::instance();
        prefs.setCodeFoldingEnabled(false);
        prefs.setBracketAssistanceEnabled(false);
        prefs.setSmartIndentationEnabled(false);
        prefs.setDocumentCompletionEnabled(false);
        manager.applyCodeFeaturePreferences();

        QCOMPARE(editor->folding(), CustomEditor::NoFoldStyle);
        QVERIFY(!editor->bracketAssistanceEnabled());
        QCOMPARE(editor->braceMatching(), CustomEditor::NoBraceMatch);
        QVERIFY(!editor->indentationGuides());
        QVERIFY(!editor->tabIndents());
        QVERIFY(!editor->backspaceUnindents());
        QVERIFY(!editor->autoIndent());
        QVERIFY(!editor->documentCompletionEnabled());
        QCOMPARE(editor->autoCompletionSource(), CustomEditor::AcsNone);
    }

    void togglesCurrentCodeFold()
    {
        EditorManager manager;
        CustomEditor *editor = manager.createEditor();
        manager.updateEditorSyntax(editor, QStringLiteral("example.cpp"));
        editor->setText(QStringLiteral("int main() {\n    return 0;\n}\n"));
        editor->setCursorPosition(0, 0);

        editor->toggleCurrentFold();
        QVERIFY(!editor->SendScintilla(QsciScintillaBase::SCI_GETFOLDEXPANDED, 0));

        editor->toggleCurrentFold();
        QVERIFY(editor->SendScintilla(QsciScintillaBase::SCI_GETFOLDEXPANDED, 0));
    }

    void closingImmediatelyAfterSaveWaitsForSnapshot()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("close-after-save.txt"));

        EditorPane pane;
        DocumentController controller(&pane, false);
        controller.createDefaultDocument();
        CustomEditor *editor = controller.currentEditor();
        QVERIFY(editor);
        editor->setText(QStringLiteral("content that must survive closing"));

        QVERIFY(controller.saveFile(editor, path));
        controller.requestClose(0);

        QCOMPARE(pane.count(), 0);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("content that must survive closing"));
    }

    void sharedEditorsUseOneDocumentSession()
    {
        EditorManager manager;
        CustomEditor *source = manager.createEditor(QStringLiteral("source"));
        CustomEditor *shared = manager.createEditorSharingDocument(source, QStringLiteral("shared"));
        QVERIFY(source);
        QVERIFY(shared);

        const DocumentFormat format{DocumentEncoding::Utf16LittleEndian,
                                    DocumentLineEnding::CrLf};
        source->setDocumentFormat(format);
        source->setExternalConflict(true, false);

        QCOMPARE(shared->documentFormat().encoding, DocumentEncoding::Utf16LittleEndian);
        QCOMPARE(shared->documentFormat().lineEnding, DocumentLineEnding::CrLf);
        QVERIFY(shared->hasExternalConflict());

        shared->setExternalConflict(false);
        QVERIFY(!source->hasExternalConflict());
    }

    void loadsEmbeddedNullWithoutTruncatingTheEditorBuffer()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("embedded-null.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArray("a\0b", 3)), qint64(3));
        file.close();

        EditorPane pane;
        DocumentController controller(&pane, false);
        controller.openFile(path);
        QTRY_VERIFY_WITH_TIMEOUT(controller.currentEditor() != nullptr, 3000);
        CustomEditor *editor = controller.currentEditor();
        QTRY_COMPARE_WITH_TIMEOUT(
            editor->SendScintilla(QsciScintillaBase::SCI_GETLENGTH), 3L, 3000);
        QCOMPARE(editor->SendScintilla(QsciScintillaBase::SCI_GETCHARAT, 1UL), 0L);
        QCOMPARE(editor->SendScintilla(QsciScintillaBase::SCI_GETCHARAT, 2UL), long('b'));
    }

    void coalescesRapidHighlightRefreshes()
    {
        CustomEditor editor;
        editor.setText(QString(5000, QLatin1Char('a')));
        FindReplaceManager manager;
        manager.setCurrentEditor(&editor);
        QSignalSpy refreshed(&manager, &FindReplaceManager::highlightsRefreshed);

        manager.setFindText(QStringLiteral("a"));
        manager.setFindText(QStringLiteral("aa"));
        manager.setFindText(QStringLiteral("aaa"));

        QTRY_COMPARE_WITH_TIMEOUT(refreshed.count(), 1, 1000);
    }

    void openingTheSameFileTwiceReusesThePage()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("same.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("same content"), qint64(12));
        file.close();

        EditorPane pane;
        DocumentController controller(&pane, false);
        controller.openFile(path);
        QTRY_COMPARE_WITH_TIMEOUT(pane.count(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(controller.currentEditor() && !controller.currentEditor()->isReadOnly(), 3000);

        controller.openFile(path);
        QCOMPARE(pane.count(), 1);
    }

    void splitCoordinatorSharesTheOpenDocument()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("shared.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("shared content"), qint64(14));
        file.close();

        QSplitter splitter;
        EditorPane leftPane(&splitter);
        EditorPane rightPane(&splitter);
        DocumentSaveCoordinator saveCoordinator;
        ExternalFileWatcher fileWatcher;
        DocumentController left(&leftPane, false, &saveCoordinator, &fileWatcher);
        DocumentController right(&rightPane, false, &saveCoordinator, &fileWatcher);
        SplitEditorCoordinator coordinator(&left, &right, &splitter, false, &fileWatcher);

        left.openFile(path);
        QTRY_VERIFY_WITH_TIMEOUT(left.currentEditor() && !left.currentEditor()->isReadOnly(), 3000);
        coordinator.setActiveController(&left);
        coordinator.setSplitViewEnabled(true);

        QTRY_COMPARE_WITH_TIMEOUT(rightPane.count(), 1, 3000);
        QVERIFY(right.currentEditor());
        QCOMPARE(left.currentEditor()->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER),
                 right.currentEditor()->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER));
    }

    void externalFileWatcherReportsChangedMetadata()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("watched.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("old"), qint64(3));
        file.close();

        QObject owner;
        ExternalFileWatcher watcher;
        watcher.setTrackedFilePaths(&owner, {path});
        watcher.recordFileState(path);
        QSignalSpy changed(&watcher, &ExternalFileWatcher::externalFileChanged);

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("new content"), qint64(11));
        file.close();
        watcher.requestExternalChangeCheck(path);

        QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, 3000);
        QCOMPARE(changed.first().first().toString(), DocumentPath::normalized(path));
    }

    void coalescesStaleMarkdownRenders()
    {
        MarkdownPreviewer previewer;
        QVector<quint64> completed;
        connect(&previewer, &MarkdownPreviewer::previewRendered,
                this, [&completed](const MarkdownRenderResult &result) {
            completed.append(result.requestId);
        });

        const quint64 first = previewer.renderPreviewAsync(QStringLiteral("# first"), QStringLiteral("a.md"));
        previewer.renderPreviewAsync(QStringLiteral("# obsolete"), QStringLiteral("a.md"));
        const quint64 latest = previewer.renderPreviewAsync(QStringLiteral("# latest"), QStringLiteral("a.md"));

        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 2, 3000);
        QCOMPARE(completed.first(), first);
        QCOMPARE(completed.last(), latest);
    }

    void preservesLatestMarkdownRenderForEachPage()
    {
        MarkdownPreviewer previewer;
        QObject firstPage;
        QObject secondPage;
        QVector<quint64> completed;
        connect(&previewer, &MarkdownPreviewer::previewRendered,
                this, [&completed](const MarkdownRenderResult &result) {
            completed.append(result.requestId);
        });

        const quint64 first = previewer.renderPreviewAsync(
            QStringLiteral("# first"), QStringLiteral("first.md"), &firstPage);
        const quint64 second = previewer.renderPreviewAsync(
            QStringLiteral("# second"), QStringLiteral("second.md"), &secondPage);
        previewer.renderPreviewAsync(
            QStringLiteral("# obsolete"), QStringLiteral("first.md"), &firstPage);
        const quint64 latestFirst = previewer.renderPreviewAsync(
            QStringLiteral("# latest first"), QStringLiteral("first.md"), &firstPage);

        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 3, 3000);
        QCOMPARE(completed, QVector<quint64>({first, second, latestFirst}));
    }

    void sanitizesUnsafeMarkdownContent()
    {
        MarkdownPreviewer previewer;
        QString renderedHtml;
        connect(&previewer, &MarkdownPreviewer::previewRendered,
                this, [&renderedHtml](const MarkdownRenderResult &result) {
            renderedHtml = result.htmlDocument;
        });

        previewer.renderPreviewAsync(
            QStringLiteral("<script>alert('unsafe')</script>\n\n[link](javascript:alert('unsafe'))"),
            QStringLiteral("unsafe.md"));

        QTRY_VERIFY_WITH_TIMEOUT(!renderedHtml.isEmpty(), 3000);
        QVERIFY(!renderedHtml.contains(QStringLiteral("<script"), Qt::CaseInsensitive));
        QVERIFY(!renderedHtml.contains(QStringLiteral("javascript:"), Qt::CaseInsensitive));
    }
};

QTEST_MAIN(EditorFeaturesTest)
#include "editorfeatures_test.moc"
