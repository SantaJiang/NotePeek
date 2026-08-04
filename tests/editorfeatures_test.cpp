#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/documentpath.h"
#include "editor/documentsavecoordinator.h"
#include "editor/editorpane.h"
#include "editor/editormanager.h"
#include "editor/externalfilewatcher.h"
#include "editor/spliteditorcoordinator.h"
#include "decorators/highlightedscrollbar.h"
#include "decorators/highlightedscrollbardecorator.h"
#include "find/findreplacemanager.h"
#include "tool/markdown/markdownpreviewer.h"
#include "widgets/elidingcombobox.h"
#include "apppreferences.h"
#include <NotePeek/semantictokens.h>

#include <QFile>
#include <QKeyEvent>
#include <QScrollBar>
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
        prefs.setSemanticHighlightingEnabled(true);
    }

    void cleanup()
    {
        AppPreferences &prefs = AppPreferences::instance();
        prefs.setCodeFoldingEnabled(false);
        prefs.setBracketAssistanceEnabled(false);
        prefs.setSmartIndentationEnabled(true);
        prefs.setDocumentCompletionEnabled(false);
        prefs.setSemanticHighlightingEnabled(true);
    }

    void elidesLongCurrentFileName()
    {
        ElidingComboBox combo;
        const QString longName =
            QStringLiteral("develop-for-6.0-2503-sync-memory.md");
        combo.addItem(longName);
        combo.resize(230, 25);
        combo.setStyleSheet(QStringLiteral(
            "QComboBox { padding: 0 0 0 8px; }"
            "QComboBox::drop-down { width: 22px; }"));

        QStyleOptionComboBox option;
        option.initFrom(&combo);
        const QRect textRect = combo.style()->subControlRect(QStyle::CC_ComboBox,
                                                              &option,
                                                              QStyle::SC_ComboBoxEditField,
                                                              &combo);
        QVERIFY(textRect.width() > 190);

        const QString displayedText = combo.elidedCurrentText();
        QVERIFY(displayedText != longName);
        QVERIFY(displayedText.endsWith(QChar(0x2026)));
        QVERIFY(combo.fontMetrics().horizontalAdvance(displayedText)
                < combo.fontMetrics().horizontalAdvance(longName));
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

    void untitledDocumentFollowsConfiguredMode()
    {
        AppPreferences &prefs = AppPreferences::instance();
        const bool originalMode = prefs.untitledUsesCodeMode();

        // Text mode is the default: prose-style wrapping, no line-number margin.
        prefs.setUntitledUsesCodeMode(false);
        EditorPane textPane;
        DocumentController textController(&textPane, false);
        textController.createDefaultDocument();
        CustomEditor *textEditor = textController.currentEditor();
        QVERIFY(textEditor);
        QVERIFY(textEditor->filePath().isEmpty());
        QCOMPARE(textEditor->wrapMode(), CustomEditor::WrapWord);
        QCOMPARE(textEditor->marginWidth(0), 0);

        // Code mode keeps line numbers and horizontal scrolling.
        prefs.setUntitledUsesCodeMode(true);
        EditorPane codePane;
        DocumentController codeController(&codePane, false);
        codeController.createDefaultDocument();
        CustomEditor *codeEditor = codeController.currentEditor();
        QVERIFY(codeEditor);
        QCOMPARE(codeEditor->wrapMode(), CustomEditor::WrapNone);
        QVERIFY(codeEditor->marginWidth(0) > 0);

        // Changing the preference re-applies to already open untitled documents.
        prefs.setUntitledUsesCodeMode(false);
        codeController.editorManager()->applyCodeFeaturePreferences();
        QCOMPARE(codeEditor->wrapMode(), CustomEditor::WrapWord);
        QCOMPARE(codeEditor->marginWidth(0), 0);

        prefs.setUntitledUsesCodeMode(originalMode);
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

    void rendersSemanticTokensAtUtf16PositionsAndRejectsStaleResults()
    {
        CustomEditor editor;
        editor.setText(QStringLiteral("éfoo()\n"));
        NotePeek::SyntaxPalette colors;
        colors.insert(NotePeek::SyntaxRole::Function, QColor(QStringLiteral("#12AB34")));
        editor.setSemanticTokenColors(colors);
        const quint64 revision = editor.documentRevision();
        QVERIFY(editor.setSemanticTokens(revision, {{0, 1, 3, NotePeek::SyntaxRole::Function}}));
        QCOMPARE(editor.semanticTokens().size(), 1);

        // 'é' occupies one UTF-16 code unit but two UTF-8 bytes in Scintilla.
        const long value = editor.SendScintilla(QsciScintillaBase::SCI_INDICATORVALUEAT, 8, 2);
        QVERIFY((value & QsciScintillaBase::SC_INDICVALUEBIT) != 0);
        QCOMPARE(value & QsciScintillaBase::SC_INDICVALUEMASK, 0x34AB12L);

        editor.append(QStringLiteral("changed"));
        QVERIFY(editor.semanticTokens().isEmpty());
        QVERIFY(!editor.setSemanticTokens(revision, {{0, 1, 3, NotePeek::SyntaxRole::Function}}));
    }

    void shiftsCachedSemanticTokensIncrementallyAfterLineInsertion()
    {
        CustomEditor editor;
        editor.setText(QStringLiteral("first\nsecond\nthird\n"));
        const quint64 revision = editor.documentRevision();
        QVERIFY(editor.setSemanticTokens(revision, {
            {0, 0, 5, NotePeek::SyntaxRole::Function},
            {1, 0, 6, NotePeek::SyntaxRole::Variable},
            {2, 0, 5, NotePeek::SyntaxRole::Property}
        }));

        editor.setCursorPosition(0, 0);
        editor.insert(QStringLiteral("new\n"));

        const QVector<NotePeek::SemanticToken> shifted = editor.semanticTokens();
        QCOMPARE(shifted.size(), 2);
        QCOMPARE(shifted.at(0).line, 2);
        QCOMPARE(shifted.at(1).line, 3);
        QCOMPARE(shifted.at(0).role, NotePeek::SyntaxRole::Variable);
        QCOMPARE(shifted.at(1).role, NotePeek::SyntaxRole::Property);
    }

    void semanticServiceIsIdleForHiddenEditorsAndCoalescesNavigation()
    {
        EditorManager manager;
        CustomEditor *editor = manager.createEditor();
        editor->resize(800, 600);
        editor->setText(QStringLiteral("Widget createWidget() { return object.title; }\n"));
        manager.updateEditorSyntax(editor, QStringLiteral("idle.cpp"));

        QTest::qWait(180);
        QCOMPARE(editor->property("semanticStartedRequestCount").toInt(), 0);
        QCOMPARE(editor->semanticTokenCount(), 0);

        editor->show();
        QTRY_VERIFY_WITH_TIMEOUT(editor->property("semanticStartedRequestCount").toInt() > 0, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!editor->property("semanticAnalysisInFlight").toBool(), 1000);
        const int beforeNavigation = editor->property("semanticStartedRequestCount").toInt();

        for (int i = 0; i < 100; ++i)
        {
            QKeyEvent keyRelease(QEvent::KeyRelease, Qt::Key_Down, Qt::NoModifier);
            QApplication::sendEvent(editor->viewport(), &keyRelease);
        }
        QCOMPARE(editor->property("semanticStartedRequestCount").toInt(), beforeNavigation);
        QTRY_COMPARE_WITH_TIMEOUT(editor->property("semanticStartedRequestCount").toInt(),
                                  beforeNavigation + 1, 1000);

        editor->hide();
        const int beforeHiddenEdit = editor->property("semanticStartedRequestCount").toInt();
        editor->append(QStringLiteral("int hiddenEdit = 1;\n"));
        QTest::qWait(180);
        QCOMPARE(editor->property("semanticStartedRequestCount").toInt(), beforeHiddenEdit);
    }

    void semanticHighlightingCanBeDisabledWithoutBackgroundWork()
    {
        EditorManager manager;
        CustomEditor *editor = manager.createEditor();
        editor->resize(800, 600);
        editor->show();
        editor->setText(QStringLiteral("Widget createWidget() { return object.title; }\n"));
        manager.updateEditorSyntax(editor, QStringLiteral("disabled.cpp"));
        QTRY_VERIFY_WITH_TIMEOUT(editor->semanticTokenCount() > 0, 1000);

        AppPreferences::instance().setSemanticHighlightingEnabled(false);
        manager.reapplyThemeToAllEditors();
        QCOMPARE(editor->semanticTokenCount(), 0);
        const int started = editor->property("semanticStartedRequestCount").toInt();
        editor->append(QStringLiteral("int noWorker = 1;\n"));
        QTest::qWait(180);
        QCOMPARE(editor->property("semanticStartedRequestCount").toInt(), started);
        QVERIFY(!editor->property("semanticAnalysisInFlight").toBool());
    }

    void scrollBarMarkersPreserveNativeQScintillaInteraction()
    {
        CustomEditor editor;
        QString text;
        for (int line = 0; line < 500; ++line)
            text.append(QStringLiteral("line %1\n").arg(line));
        editor.setText(text);
        editor.resize(800, 500);
        HighlightedScrollBarDecorator decorator(&editor);
        editor.show();
        QApplication::processEvents();

        HighlightedScrollBar *scrollBar = decorator.getScrollBar();
        QVERIFY(scrollBar);
        QCOMPARE(scrollBar->parentWidget(),
                 static_cast<QWidget *>(editor.verticalScrollBar()));
        QVERIFY(scrollBar->isVisible());
        QCOMPARE(scrollBar->geometry().topLeft(), QPoint(0, 0));
        QCOMPARE(scrollBar->size(), editor.verticalScrollBar()->size());
        QCOMPARE(scrollBar->mapTo(&editor, QPoint(0, 0)),
                 editor.verticalScrollBar()->mapTo(&editor, QPoint(0, 0)));
        QVERIFY(scrollBar->testAttribute(Qt::WA_TransparentForMouseEvents));
        editor.verticalScrollBar()->setValue(100);
        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE), 100L);

        editor.SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, 50L);
        QCOMPARE(editor.verticalScrollBar()->value(), 50);

        const int position = int(editor.SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, 25L));
        editor.SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT,
                             decorator.getSearchIndicator());
        editor.SendScintilla(QsciScintillaBase::SCI_INDICATORFILLRANGE, position, 4L);
        editor.markerAdd(30, decorator.getBookmarkMarker());
        editor.setCursorPosition(40, 0);
        decorator.notify();

        QCOMPARE(scrollBar->searchMarkerCount(), 1);
        QCOMPARE(scrollBar->bookmarkMarkerCount(), 1);
        QCOMPARE(scrollBar->searchMarkerLines(), QVector<int>({25}));
        QCOMPARE(scrollBar->bookmarkMarkerLines(), QVector<int>({30}));
        QTRY_COMPARE_WITH_TIMEOUT(scrollBar->currentLine(), 40, 500);

        const auto verifyMarkersAfterResize = [&](const QSize &size) {
            editor.resize(size);
            QApplication::processEvents();
            QCOMPARE(scrollBar->geometry().topLeft(), QPoint(0, 0));
            QTRY_COMPARE_WITH_TIMEOUT(scrollBar->size(),
                                      editor.verticalScrollBar()->size(), 500);
            QCOMPARE(scrollBar->mapTo(&editor, QPoint(0, 0)),
                     editor.verticalScrollBar()->mapTo(&editor, QPoint(0, 0)));
            QTRY_VERIFY_WITH_TIMEOUT(scrollBar->isVisible(), 500);
            QCOMPARE(scrollBar->searchMarkerLines(), QVector<int>({25}));
            QCOMPARE(scrollBar->bookmarkMarkerLines(), QVector<int>({30}));
            QCOMPARE(scrollBar->currentLine(), 40);
        };
        // Exercise intermediate frames instead of validating only the final
        // geometry. This models interactive resize and macOS zoom animations.
        for (int frame = 0; frame <= 12; ++frame)
            verifyMarkersAfterResize(QSize(640 + frame * 30, 360 + frame * 30));
        for (int frame = 12; frame >= 0; --frame)
            verifyMarkersAfterResize(QSize(640 + frame * 30, 360 + frame * 30));
    }

    void scrollBarMarkersAppearWhenAttachedToAnAlreadyVisibleEditor()
    {
        CustomEditor editor;
        QString text;
        for (int line = 0; line < 500; ++line)
            text.append(QStringLiteral("line %1\n").arg(line));
        editor.setText(text);
        editor.resize(800, 500);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));

        // Attaching after the first show() is the case that silently produced
        // an invisible marker layer: a widget created under an already-visible
        // parent stays hidden until it is shown explicitly.
        HighlightedScrollBarDecorator decorator(&editor);
        QApplication::processEvents();

        HighlightedScrollBar *scrollBar = decorator.getScrollBar();
        QVERIFY(scrollBar);
        QTRY_VERIFY_WITH_TIMEOUT(scrollBar->isVisible(), 500);
        QCOMPARE(scrollBar->size(), editor.verticalScrollBar()->size());
    }

    void evictsSemanticTokensFurthestFromTheRetainedWindow()
    {
        CustomEditor editor;
        QString text;
        for (int line = 0; line < 300; ++line)
            text.append(QStringLiteral("alpha bravo charlie\n"));
        editor.setText(text);

        NotePeek::SyntaxPalette colors;
        colors.insert(NotePeek::SyntaxRole::Function, QColor(QStringLiteral("#12AB34")));
        editor.setSemanticTokenColors(colors);

        QVector<NotePeek::SemanticToken> tokens;
        for (int line = 0; line < 300; ++line)
            tokens.append({line, 0, 5, NotePeek::SyntaxRole::Function});
        QVERIFY(editor.setSemanticTokens(editor.documentRevision(), tokens));
        QCOMPARE(editor.semanticTokenCount(), 300);

        editor.evictSemanticTokensOutside(140, 160, 40);
        QVERIFY(editor.semanticTokenCount() <= 40);
        QVERIFY(editor.semanticTokenCount() > 0);

        // The retained window must survive; only distant lines are dropped.
        bool keptWindowStart = false;
        bool keptWindowEnd = false;
        for (const NotePeek::SemanticToken &token : editor.semanticTokens())
        {
            keptWindowStart |= token.line == 140;
            keptWindowEnd |= token.line == 160;
            QVERIFY(token.line > 100);
            QVERIFY(token.line < 200);
        }
        QVERIFY(keptWindowStart);
        QVERIFY(keptWindowEnd);
    }

    void editorManagerRefreshesSemanticHighlightingAfterEdits()
    {
        EditorManager manager;
        CustomEditor *editor = manager.createEditor();
        editor->resize(800, 600);
        editor->show();
        manager.updateEditorSyntax(editor, QStringLiteral("semantic.cpp"));
        editor->setText(QStringLiteral("Widget createWidget() { return object.title; }\n"));
        QTRY_VERIFY_WITH_TIMEOUT(!editor->semanticTokens().isEmpty(), 1000);

        bool hasFunction = false;
        bool hasProperty = false;
        for (const NotePeek::SemanticToken &token : editor->semanticTokens())
        {
            const QString value = editor->text(token.line).mid(token.utf16Column, token.utf16Length);
            hasFunction |= value == QLatin1String("createWidget")
                           && token.role == NotePeek::SyntaxRole::Function;
            hasProperty |= value == QLatin1String("title")
                           && token.role == NotePeek::SyntaxRole::Property;
        }
        QVERIFY(hasFunction);
        QVERIFY(hasProperty);

        editor->setText(QStringLiteral("int renamedVariable = 1;\n"));
        QVERIFY(editor->semanticTokens().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!editor->semanticTokens().isEmpty(), 1000);
    }
};

QTEST_MAIN(EditorFeaturesTest)
#include "editorfeatures_test.moc"
