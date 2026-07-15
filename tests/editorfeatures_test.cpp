#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/editorpane.h"
#include "editor/editormanager.h"
#include "apppreferences.h"

#include <QFile>
#include <QTemporaryDir>
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
};

QTEST_MAIN(EditorFeaturesTest)
#include "editorfeatures_test.moc"
