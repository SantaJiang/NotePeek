#include <QMenu>
#include <QDockWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>

#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/documentformat.h"
#include "editor/documentsavecoordinator.h"
#include "editor/editorpane.h"
#include "editor/externalfilewatcher.h"
#include "editor/lexerfactory.h"
#include "find/filesearcher.h"
#include "find/findreplacemanager.h"
#include "mainwindow.h"
#include "tool/cpp/headersourcemanager.h"
#include "widgets/searchresultsdock.h"

namespace {

void writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    QVERIFY2(file.open(QFile::WriteOnly), qPrintable(path));
    QCOMPARE(file.write(content), qint64(content.size()));
}

} // namespace

class NewFeaturesTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Qt 5's QSignalSpy needs the custom result type registered to record it.
        qRegisterMetaType<FileSearchOutcome>("FileSearchOutcome");
    }

    // Constructing the shell wires up docks, menus and the pane controllers in
    // a fixed order; a member used before it is built crashes here rather than
    // in front of a user.
    void constructsMainWindowShell()
    {
        MainWindow blankWindow;
        blankWindow.show();
        QVERIFY(blankWindow.isVisible());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("opened.txt"));
        writeFile(filePath, QByteArrayLiteral("hello\n"));

        MainWindow fileWindow(nullptr, { filePath });
        fileWindow.show();
        QVERIFY(fileWindow.isVisible());
        QVERIFY(fileWindow.findChild<QMenu *>() != nullptr);
    }

    void usesUntranslatedNameForUntitledDocuments()
    {
        MainWindow window;

        EditorPane *defaultPane = nullptr;
        for (EditorPane *pane : window.findChildren<EditorPane *>())
        {
            if (pane->count() > 0)
            {
                defaultPane = pane;
                break;
            }
        }

        QVERIFY(defaultPane);
        QCOMPARE(defaultPane->pageTitle(defaultPane->currentIndex()),
                 QStringLiteral("untitled"));
        QVERIFY(window.windowTitle().contains(QStringLiteral("untitled")));
    }

    void aboutDialogIsWindowModal()
    {
        MainWindow window;
        window.show();

        bool foundAboutDialog = false;
        Qt::WindowModality modality = Qt::NonModal;
        QWidget *dialogParent = nullptr;
        QTimer::singleShot(0, [&]() {
            for (QWidget *widget : QApplication::topLevelWidgets())
            {
                auto *messageBox = qobject_cast<QMessageBox *>(widget);
                if (!messageBox || !messageBox->isVisible())
                {
                    continue;
                }

                foundAboutDialog = true;
                modality = messageBox->windowModality();
                dialogParent = messageBox->parentWidget();
                messageBox->accept();
                break;
            }
        });

        QVERIFY(QMetaObject::invokeMethod(&window,
                                          "on_action_about_triggered",
                                          Qt::DirectConnection));
        QVERIFY(foundAboutDialog);
        QCOMPARE(modality, Qt::WindowModal);
        QCOMPARE(dialogParent, static_cast<QWidget *>(&window));
    }

    void searchesAcrossDirectoryTree()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("sub")));
        writeFile(root.filePath(QStringLiteral("alpha.txt")),
                  QByteArrayLiteral("needle here\nno match\nanother needle\n"));
        writeFile(root.filePath(QStringLiteral("sub/beta.cpp")),
                  QByteArrayLiteral("int needle = 0;\n"));
        writeFile(root.filePath(QStringLiteral("binary.bin")),
                  QByteArray("nee\x00dle", 7));

        FileSearcher searcher;
        QSignalSpy finished(&searcher, &FileSearcher::searchFinished);
        searcher.start(root.path(), QStringLiteral("needle"), false, false);
        QTRY_COMPARE(finished.count(), 1);

        const auto outcome = finished.takeFirst().first().value<FileSearchOutcome>();
        QCOMPARE(outcome.summary.matchCount, 3);
        QVERIFY(!outcome.summary.truncated);
        QVERIFY(!outcome.summary.canceled);

        int alphaMatches = 0;
        for (const FileSearchMatch &match : outcome.matches)
        {
            // The .bin file must never contribute: unsupported type.
            QVERIFY(!match.filePath.endsWith(QStringLiteral(".bin")));
            if (match.filePath.endsWith(QStringLiteral("alpha.txt")))
            {
                ++alphaMatches;
            }
        }
        QCOMPARE(alphaMatches, 2);
    }

    void searchResultDoubleClickActivatesOnlyOnce()
    {
        QMainWindow window;
        SearchResultsDock dock(&window);
        window.addDockWidget(Qt::BottomDockWidgetArea, dock.dockWidget());

        QTreeWidget *tree = dock.dockWidget()->findChild<QTreeWidget *>();
        QVERIFY(tree);
        auto *fileItem = new QTreeWidgetItem(tree);
        auto *matchItem = new QTreeWidgetItem(fileItem);
        matchItem->setData(0, Qt::UserRole, QStringLiteral("/tmp/result.txt"));
        matchItem->setData(0, Qt::UserRole + 1, 2);
        matchItem->setData(0, Qt::UserRole + 2, 3);
        matchItem->setData(0, Qt::UserRole + 3, 4);

        QSignalSpy activated(&dock, &SearchResultsDock::matchActivated);
        // QTreeWidget emits itemActivated as part of a platform activation;
        // a double click additionally emits itemDoubleClicked. The dock must
        // listen to only one of those signals.
        emit tree->itemActivated(matchItem, 0);
        emit tree->itemDoubleClicked(matchItem, 0);
        QCOMPARE(activated.count(), 1);
    }

    void searchJumpWaitsForAnEditorThatIsStillLoading()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("large-search-result.txt"));
        writeFile(filePath, QByteArrayLiteral("first line\nneedle target\n"));

        MainWindow window;
        window.openFileByArg(filePath);
        SearchResultsDock *dock = window.findChild<SearchResultsDock *>();
        QVERIFY(dock);

        DocumentController *loadingController = nullptr;
        CustomEditor *loadingEditor = nullptr;
        for (DocumentController *controller : window.findChildren<DocumentController *>())
        {
            CustomEditor *candidate = controller->currentEditor();
            if (candidate && candidate->filePath() == filePath)
            {
                loadingController = controller;
                loadingEditor = candidate;
                break;
            }
        }
        QVERIFY(loadingController);
        QVERIFY(loadingEditor);
        QVERIFY(loadingController->isEditorLoading(loadingEditor));

        emit dock->matchActivated(filePath, 1, 0, 6);
        QTRY_VERIFY_WITH_TIMEOUT(!loadingController->isEditorLoading(loadingEditor), 10000);
        QCOMPARE(loadingEditor->selectedText(), QStringLiteral("needle"));
    }

    void searchHonorsCaseAndWholeWord()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        writeFile(root.filePath(QStringLiteral("data.txt")),
                  QByteArrayLiteral("Value value valueOf\n"));

        FileSearcher searcher;
        QSignalSpy finished(&searcher, &FileSearcher::searchFinished);

        searcher.start(root.path(), QStringLiteral("value"), true, false);
        QTRY_COMPARE(finished.count(), 1);
        auto outcome = finished.takeFirst().first().value<FileSearchOutcome>();
        QCOMPARE(outcome.summary.matchCount, 2); // "value" and "valueOf"

        searcher.start(root.path(), QStringLiteral("value"), true, true);
        QTRY_COMPARE(finished.count(), 1);
        outcome = finished.takeFirst().first().value<FileSearchOutcome>();
        QCOMPARE(outcome.summary.matchCount, 1);
        QCOMPARE(outcome.matches.first().line, 0);
        QCOMPARE(outcome.matches.first().column, 6);
    }

    void decodesWithForcedEncoding()
    {
        // UTF-16 LE without a BOM cannot be auto-detected, but decodes when
        // the user forces the encoding.
        const QByteArray utf16le = QByteArrayLiteral("h\x00i\x00");
        QString text;
        DocumentFormat format;
        QVERIFY(!DocumentFormatCodec::decode(utf16le, &text, &format, nullptr)
                || text != QStringLiteral("hi"));
        QVERIFY(DocumentFormatCodec::decodeWithEncoding(
            utf16le, DocumentEncoding::Utf16LittleEndian, &text, &format, nullptr));
        QCOMPARE(text, QStringLiteral("hi"));
        QCOMPARE(format.encoding, DocumentEncoding::Utf16LittleEndian);

        // A matching BOM is consumed rather than leaking into the text.
        const QByteArray utf16leBom = QByteArrayLiteral("\xff\xfeh\x00i\x00");
        QVERIFY(DocumentFormatCodec::decodeWithEncoding(
            utf16leBom, DocumentEncoding::Utf16LittleEndian, &text, &format, nullptr));
        QCOMPARE(text, QStringLiteral("hi"));

        // Forcing UTF-8 on a BOM-marked file keeps the BOM encoding recorded.
        const QByteArray utf8Bom = QByteArrayLiteral("\xef\xbb\xbfhi");
        QVERIFY(DocumentFormatCodec::decodeWithEncoding(
            utf8Bom, DocumentEncoding::Utf8, &text, &format, nullptr));
        QCOMPARE(text, QStringLiteral("hi"));
        QCOMPARE(format.encoding, DocumentEncoding::Utf8Bom);

        // Invalid UTF-8 is rejected with an error.
        QString errorMessage;
        QVERIFY(!DocumentFormatCodec::decodeWithEncoding(
            QByteArrayLiteral("\xff\xff"), DocumentEncoding::Utf8, &text, &format, &errorMessage));
        QVERIFY(!errorMessage.isEmpty());

        // Odd-length UTF-16 payloads are rejected.
        QVERIFY(!DocumentFormatCodec::decodeWithEncoding(
            QByteArrayLiteral("abc"), DocumentEncoding::Utf16BigEndian, &text, &format, nullptr));
    }

    void encodesUtf16RoundTrip()
    {
        DocumentFormat format;
        format.encoding = DocumentEncoding::Utf16BigEndian;
        format.lineEnding = DocumentLineEnding::CrLf;
        const QString source = QStringLiteral("héllo\nwörld");
        const QByteArray encoded = DocumentFormatCodec::encode(source, format);
        QVERIFY(encoded.startsWith(QByteArrayLiteral("\xfe\xff")));

        QString decoded;
        DocumentFormat decodedFormat;
        QVERIFY(DocumentFormatCodec::decode(encoded, &decoded, &decodedFormat, nullptr));
        QCOMPARE(decoded, source);
        QCOMPARE(decodedFormat.encoding, DocumentEncoding::Utf16BigEndian);
        QCOMPARE(decodedFormat.lineEnding, DocumentLineEnding::CrLf);
    }

    void encodingReopenDoesNotOverwriteAConcurrentSharedEdit()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("shared-reopen.txt"));
        writeFile(filePath, QByteArrayLiteral("original\n"));

        DocumentSaveCoordinator saveCoordinator;
        ExternalFileWatcher fileWatcher;
        EditorPane firstPane;
        EditorPane secondPane;
        DocumentController first(&firstPane, false, &saveCoordinator, &fileWatcher);
        DocumentController second(&secondPane, false, &saveCoordinator, &fileWatcher);

        QSignalSpy opened(&first, &DocumentController::fileOpened);
        first.openFile(filePath);
        QTRY_COMPARE_WITH_TIMEOUT(opened.count(), 1, 5000);
        CustomEditor *source = first.currentEditor();
        QVERIFY(source);
        second.openSharedDocument(source);
        CustomEditor *shared = second.currentEditor();
        QVERIFY(shared);

        first.reopenCurrentWithEncoding(DocumentEncoding::Utf8);
        QVERIFY(source->isReadOnly());
        shared->setText(QStringLiteral("edited from the shared view\n"));

        QTRY_VERIFY_WITH_TIMEOUT(!source->isReadOnly(), 5000);
        QCOMPARE(source->text(), QStringLiteral("edited from the shared view\n"));
        QVERIFY(source->isModified());
    }

    void failedEncodingConversionRestoresThePreviousFormat()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        EditorPane pane;
        DocumentController controller(&pane, false);
        controller.createDefaultDocument();
        CustomEditor *editor = controller.currentEditor();
        QVERIFY(editor);
        editor->setText(QStringLiteral("content\n"));
        // QSaveFile cannot replace a directory, so this deterministically
        // exercises the asynchronous save-failure path.
        editor->setFilePath(dir.path());
        const DocumentFormat before = editor->documentFormat();

        QTimer messageCloser;
        connect(&messageCloser, &QTimer::timeout, []() {
            for (QWidget *widget : QApplication::topLevelWidgets())
            {
                if (auto *message = qobject_cast<QMessageBox *>(widget))
                    message->accept();
            }
        });
        messageCloser.start(10);

        QVERIFY(controller.convertEncodingAndSave(DocumentEncoding::Utf16LittleEndian));
        controller.waitForPendingSaves();
        messageCloser.stop();

        QCOMPARE(editor->documentFormat().encoding, before.encoding);
        QCOMPARE(editor->documentFormat().lineEnding, before.lineEnding);
        QVERIFY(editor->isModified());
        editor->undo();
        QVERIFY(editor->isModified());
    }

    void forcedDirtyStateSurvivesUndoingToScintillasSavePoint()
    {
        CustomEditor editor;
        editor.setText(QStringLiteral("saved"));
        editor.setModified(false);
        editor.setCursorPosition(0, 5);
        editor.insert(QStringLiteral("!"));
        QVERIFY(editor.QsciScintilla::isModified());

        editor.markModified();
        editor.undo();

        QVERIFY(!editor.QsciScintilla::isModified());
        QVERIFY(editor.isModified());
    }

    void mapsLanguagesFromPaths()
    {
        QCOMPARE(LexerFactory::languageFromFilePath(QStringLiteral("/x/main.cpp")), LexerFactory::CPP);
        QCOMPARE(LexerFactory::languageFromFilePath(QStringLiteral("/x/tool.py")), LexerFactory::Python);
        QCOMPARE(LexerFactory::languageFromFilePath(QStringLiteral("/x/notes.md")), LexerFactory::MARKDOWN);
        QCOMPARE(LexerFactory::languageFromFilePath(QStringLiteral("/x/Dockerfile")), LexerFactory::DOCKERFILE);
        QCOMPARE(LexerFactory::languageFromFilePath(QStringLiteral("/x/CMakeLists.txt")), LexerFactory::CMAKE);
        QCOMPARE(LexerFactory::languageFromFilePath(QStringLiteral("/x/unknown.zzz")), LexerFactory::Unknown);
    }

    void findsHeaderSourceCounterparts()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString headerPath = dir.filePath(QStringLiteral("widget.h"));
        const QString sourcePath = dir.filePath(QStringLiteral("widget.cpp"));
        writeFile(headerPath, QByteArrayLiteral("#pragma once\n"));
        writeFile(sourcePath, QByteArrayLiteral("#include \"widget.h\"\n"));

        HeaderSourceManager manager;
        QVERIFY(manager.isCppFile(headerPath));
        QVERIFY(manager.isHeaderFile(headerPath));
        QVERIFY(manager.isSourceFile(sourcePath));
        QCOMPARE(QFileInfo(manager.getCorrespondingFile(headerPath)).fileName(),
                 QStringLiteral("widget.cpp"));
        QCOMPARE(QFileInfo(manager.getCorrespondingFile(sourcePath)).fileName(),
                 QStringLiteral("widget.h"));
        QVERIFY(!manager.hasCorrespondingFile(dir.filePath(QStringLiteral("orphan.cpp"))));
    }

    void highlightsAllMatchesInEditor()
    {
        CustomEditor editor;
        editor.setText(QStringLiteral("foo bar foo\nbaz foo\n"));

        FindReplaceManager manager;
        manager.setCurrentEditor(&editor);
        manager.setFindText(QStringLiteral("foo"));
        manager.highlightAllMatches();
        QCOMPARE(manager.searchResults().size(), 3);

        manager.setCaseSensitive(true);
        manager.setFindText(QStringLiteral("FOO"));
        manager.highlightAllMatches();
        QCOMPARE(manager.searchResults().size(), 0);

        manager.clearHighlights();
        QCOMPARE(manager.searchResults().size(), 0);
    }
};

QTEST_MAIN(NewFeaturesTest)
#include "newfeatures_test.moc"
