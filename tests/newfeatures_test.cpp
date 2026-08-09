#include <QMenu>
#include <QComboBox>
#include <QDockWidget>
#include <QMainWindow>
#include <QLineEdit>
#include <QMessageBox>
#include <QToolButton>
#include <QSignalSpy>
#include <QScrollBar>
#include <QTimer>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>

#include "apppreferences.h"
#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/documentformat.h"
#include "editor/documentsavecoordinator.h"
#include "editor/editorpane.h"
#include "editor/editormanager.h"
#include "editor/externalfilewatcher.h"
#include <NotePeek/lexerfactory.h>
#include <NotePeek/semantictokens.h>
#include <NotePeek/syntaxstyler.h>
#include <NotePeek/syntaxtheme.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerpython.h>
#include "find/filesearcher.h"
#include "find/findreplacemanager.h"
#include "mainwindow.h"
#include "preferencesdialog.h"
#include "tool/cpp/headersourcemanager.h"
#include "tool/markdown/emojifontsupport.h"
#include "widgets/fileexplorerdock.h"
#include "widgets/filetypefilterproxymodel.h"
#include "widgets/searchresultsdock.h"
#include <QFileSystemModel>
#include <QHeaderView>
#include <QImage>
#include <QTreeView>
#include "themecolors.h"
#include "widgets/qscieditorinfostatusbar.h"
#include "find/findreplacewidget.h"

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
        qRegisterMetaType<FileSearchOutcome>("FileSearchOutcome");
    }

    void detectsEmojiWithoutTreatingOrdinaryUnicodeAsEmoji()
    {
        QVERIFY(!QFile::exists(QStringLiteral(":/resource/NotoColorEmoji.ttf")));
        QVERIFY(!EmojiFontSupport::containsEmoji(QByteArrayLiteral("plain Markdown 中文")));
        QVERIFY(!EmojiFontSupport::containsEmoji(QStringLiteral("Copyright © 2026").toUtf8()));
        QVERIFY(EmojiFontSupport::containsEmoji(QStringLiteral("Copyright ©️ 2026").toUtf8()));
        QVERIFY(EmojiFontSupport::containsEmoji(QStringLiteral("status: ✅").toUtf8()));
        QVERIFY(EmojiFontSupport::containsEmoji(QStringLiteral("developer 👩🏽‍💻").toUtf8()));
        QVERIFY(EmojiFontSupport::shouldRecommendInstallation(
            QStringLiteral("flag 🇨🇳").toUtf8(), false));
        QVERIFY(!EmojiFontSupport::shouldRecommendInstallation(
            QStringLiteral("flag 🇨🇳").toUtf8(), true));
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

    void paletteChangeThemeRefreshDoesNotRecurse()
    {
        MainWindow window;
        auto *statusBar = window.findChild<QsciEditorInfoStatusBar *>();
        auto *findReplace = window.findChild<FindReplaceWidget *>();
        QVERIFY(statusBar);
        QVERIFY(findReplace);

        // Force each handler to install a different sheet. The installation
        // itself may synchronously deliver PaletteChange while Qt repolishes;
        // the nested event must observe the new sheet and return immediately.
        window.setStyleSheet(QStringLiteral("QMainWindow { color: red; }"));
        statusBar->setStyleSheet(QStringLiteral("QStatusBar { color: red; }"));
        findReplace->setStyleSheet(QStringLiteral("FindReplaceWidget { color: red; }"));

        QEvent windowPaletteChange(QEvent::PaletteChange);
        QCoreApplication::sendEvent(&window, &windowPaletteChange);
        QEvent statusPaletteChange(QEvent::PaletteChange);
        QCoreApplication::sendEvent(statusBar, &statusPaletteChange);
        QEvent findPaletteChange(QEvent::PaletteChange);
        QCoreApplication::sendEvent(findReplace, &findPaletteChange);

        QTRY_VERIFY(!window.styleSheet().contains(QStringLiteral("color: red")));
        QTRY_VERIFY(!statusBar->styleSheet().contains(QStringLiteral("color: red")));
        QTRY_VERIFY(!findReplace->styleSheet().contains(QStringLiteral("color: red")));

        // The queued repolish must leave native controls resolved from the
        // final application palette, not the palette from the previous scheme.
        CustomEditor *editor = window.findChild<CustomEditor *>();
        QVERIFY(editor);
        QCOMPARE(editor->verticalScrollBar()->palette().color(QPalette::Button),
                 QApplication::palette().color(QPalette::Button));
        QCOMPARE(editor->verticalScrollBar()->palette().color(QPalette::Mid),
                 QApplication::palette().color(QPalette::Mid));
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
        bool hasIcon = false;
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
                hasIcon = !messageBox->iconPixmap().isNull();
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
        QVERIFY(hasIcon);
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

    void longSearchResultPreviewKeepsMatchVisible()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString longLine = QString(900, QLatin1Char('x'))
                                 + QStringLiteral("needle")
                                 + QString(200, QLatin1Char('y'));
        writeFile(root.filePath(QStringLiteral("long.txt")),
                  (longLine + QStringLiteral("\r\n")).toUtf8());

        FileSearcher searcher;
        QSignalSpy finished(&searcher, &FileSearcher::searchFinished);
        searcher.start(root.path(), QStringLiteral("needle"), true, false);
        QTRY_COMPARE(finished.count(), 1);

        const auto outcome = finished.takeFirst().first().value<FileSearchOutcome>();
        QCOMPARE(outcome.matches.size(), 1);
        const FileSearchMatch &match = outcome.matches.constFirst();
        QCOMPARE(match.column, 900);
        QVERIFY(match.lineText.size() <= 500);
        QVERIFY(!match.lineText.contains(QLatin1Char('\r')));
        QCOMPARE(match.lineText.mid(match.previewColumn, match.length),
                 QStringLiteral("needle"));
    }

    void searchResultDelegatePaintsLineNumberAndHighlight()
    {
        QMainWindow window;
        SearchResultsDock dock(&window);
        window.addDockWidget(Qt::BottomDockWidgetArea, dock.dockWidget());
        window.resize(700, 320);

        FileSearchOutcome outcome;
        FileSearchMatch match;
        match.filePath = QStringLiteral("/tmp/result.txt");
        match.line = 41;
        match.column = 7;
        match.length = 6;
        match.previewColumn = 7;
        match.lineText = QStringLiteral("prefix needle suffix");
        outcome.matches.append(match);
        outcome.summary.matchCount = 1;

        QVERIFY(QMetaObject::invokeMethod(&dock,
                                          "onSearchFinished",
                                          Qt::DirectConnection,
                                          Q_ARG(FileSearchOutcome, outcome)));
        QTreeWidget *tree = dock.dockWidget()->findChild<QTreeWidget *>();
        QVERIFY(tree);
        QVERIFY(tree->findChild<QObject *>(QStringLiteral("searchResultDelegate")));
        QCOMPARE(tree->indentation(), 12);
        QVERIFY(tree->property("searchResultLineNumberWidth").toInt() < 64);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QTreeWidgetItem *matchItem = tree->topLevelItem(0)->child(0);
        QVERIFY(matchItem);
        QCOMPARE(matchItem->text(0), match.lineText);
        QVERIFY(matchItem->data(0, Qt::AccessibleTextRole).toString().contains(QStringLiteral("42")));

        window.show();
        QCoreApplication::processEvents();
        const QImage rendered = tree->viewport()->grab().toImage();
        QVERIFY(!rendered.isNull());
        const QRgb highlight = ThemeColors::FindReplace::highlightIndicator().rgb();
        bool foundHighlight = false;
        for (int y = 0; y < rendered.height() && !foundHighlight; ++y)
        {
            for (int x = 0; x < rendered.width(); ++x)
            {
                if (rendered.pixel(x, y) == highlight)
                {
                    foundHighlight = true;
                    break;
                }
            }
        }
        QVERIFY(foundHighlight);
    }

    void advancedSearchChromeAndEntryPointStayInSync()
    {
        MainWindow window;
        SearchResultsDock *searchDock = window.findChild<SearchResultsDock *>();
        FindReplaceWidget *findReplace = window.findChild<FindReplaceWidget *>();
        QVERIFY(searchDock);
        QVERIFY(findReplace);

        QDockWidget *dockWidget = searchDock->dockWidget();
        QVERIFY(dockWidget);
        QVERIFY(dockWidget->features().testFlag(QDockWidget::DockWidgetClosable));
        QVERIFY(!dockWidget->features().testFlag(QDockWidget::DockWidgetFloatable));
        QCOMPARE(window.corner(Qt::BottomLeftCorner), Qt::LeftDockWidgetArea);

        auto *titleBar = dockWidget->findChild<QWidget *>(QStringLiteral("searchResultsTitleBar"));
        auto *closeButton = dockWidget->findChild<QToolButton *>(QStringLiteral("searchResultsCloseButton"));
        auto *patternEdit = dockWidget->findChild<QLineEdit *>(QStringLiteral("searchResultsPatternEdit"));
        auto *caseButton = dockWidget->findChild<QToolButton *>(QStringLiteral("searchResultsCaseButton"));
        auto *wholeWordButton = dockWidget->findChild<QToolButton *>(QStringLiteral("searchResultsWholeWordButton"));
        auto *searchButton = dockWidget->findChild<QToolButton *>(QStringLiteral("searchResultsSearchButton"));
        QVERIFY(titleBar);
        QVERIFY(closeButton);
        QVERIFY(patternEdit);
        QVERIFY(caseButton);
        QVERIFY(wholeWordButton);
        QVERIFY(searchButton);
        QVERIFY(!titleBar->styleSheet().isEmpty());
        QVERIFY(!searchButton->styleSheet().isEmpty());
        QCOMPARE(caseButton->height(), patternEdit->sizeHint().height());
        QCOMPARE(wholeWordButton->height(), patternEdit->sizeHint().height());

        auto *advancedButton = findReplace->findChild<QToolButton *>(QStringLiteral("buttonAdvanced"));
        QVERIFY(advancedButton);
        QVERIFY(dockWidget->isHidden());
        advancedButton->click();
        QVERIFY(!dockWidget->isHidden());
        closeButton->click();
        QVERIFY(dockWidget->isHidden());
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

    void mapsLexerStylesThroughCustomizedLibraryRoles()
    {
        QsciLexerCPP cpp;
        QCOMPARE(NotePeek::SyntaxStyler::roleForStyle(&cpp, QsciLexerCPP::Keyword),
                 NotePeek::SyntaxRole::Keyword);
        QCOMPARE(NotePeek::SyntaxStyler::roleForStyle(&cpp, QsciLexerCPP::GlobalClass),
                 NotePeek::SyntaxRole::Type);
        QCOMPARE(NotePeek::SyntaxStyler::roleForStyle(&cpp, QsciLexerCPP::Identifier),
                 NotePeek::SyntaxRole::Default);

        QsciLexerPython python;
        QCOMPARE(NotePeek::SyntaxStyler::roleForStyle(&python, QsciLexerPython::FunctionMethodName),
                 NotePeek::SyntaxRole::Function);
        QCOMPARE(NotePeek::SyntaxStyler::roleForStyle(&python, QsciLexerPython::Decorator),
                 NotePeek::SyntaxRole::Attribute);

        QsciLexerJSON json;
        QCOMPARE(NotePeek::SyntaxStyler::roleForStyle(&json, QsciLexerJSON::Property),
                 NotePeek::SyntaxRole::Property);
        QCOMPARE(NotePeek::SyntaxStyler::roleForStyle(&json, QsciLexerJSON::EscapeSequence),
                 NotePeek::SyntaxRole::Escape);
    }

    void cppIncludeDirectivesSeparateDirectiveAndHeaderStyles()
    {
        QsciScintilla editor;
        QsciLexer *lexer = LexerFactory::createLexer(LexerFactory::CPP);
        QVERIFY(lexer);
        auto *cpp = qobject_cast<QsciLexerCPP *>(lexer);
        QVERIFY(cpp);
        QVERIFY(cpp->stylePreprocessor());
        lexer->setParent(&editor);
        editor.setLexer(lexer);

        const QString source = QStringLiteral(
            "#include \"editor/documentformat.h\"\n"
            "#include <QDir>\n");
        editor.setText(source);
        editor.SendScintilla(QsciScintillaBase::SCI_COLOURISE, 0L, -1L);

        const auto styleAt = [&](int position) {
            return int(editor.SendScintilla(QsciScintillaBase::SCI_GETSTYLEAT,
                                            position));
        };
        const int quotedDelimiter = source.indexOf(QLatin1Char('"'));
        const int quotedPath = source.indexOf(QStringLiteral("editor/documentformat.h"));
        const int angleDelimiter = source.indexOf(QLatin1Char('<'));
        const int anglePath = source.indexOf(QStringLiteral("QDir"));

        QCOMPARE(styleAt(0), int(QsciLexerCPP::PreProcessor));
        QCOMPARE(styleAt(source.indexOf(QStringLiteral("#include"), 1)),
                 int(QsciLexerCPP::PreProcessor));
        QCOMPARE(styleAt(quotedDelimiter), int(QsciLexerCPP::DoubleQuotedString));
        QCOMPARE(styleAt(quotedPath), int(QsciLexerCPP::DoubleQuotedString));
        QCOMPARE(styleAt(angleDelimiter), int(QsciLexerCPP::DoubleQuotedString));
        QCOMPARE(styleAt(anglePath), int(QsciLexerCPP::DoubleQuotedString));
        QCOMPARE(NotePeek::SyntaxStyler::roleForStyle(lexer, styleAt(0)),
                 NotePeek::SyntaxRole::Preprocessor);
        QCOMPARE(NotePeek::SyntaxStyler::roleForStyle(lexer, styleAt(quotedPath)),
                 NotePeek::SyntaxRole::String);
        QCOMPARE(NotePeek::SyntaxStyler::roleForStyle(lexer, styleAt(anglePath)),
                 NotePeek::SyntaxRole::String);
    }

    void loadsCompleteBuiltInSyntaxThemesFromJson()
    {
        const QVector<NotePeek::SyntaxTheme> themes = NotePeek::SyntaxThemeCatalog::builtInThemes();
        QCOMPARE(themes.size(), 8);
        int darkCount = 0;
        int lightCount = 0;
        QSet<QString> ids;
        for (const NotePeek::SyntaxTheme &theme : themes)
        {
            QVERIFY2(theme.isValid(), qPrintable(theme.id));
            QVERIFY(!ids.contains(theme.id));
            ids.insert(theme.id);
            theme.dark ? ++darkCount : ++lightCount;
            QCOMPARE(theme.syntaxColors.size(), 22);
            QVERIFY(theme.editorColor(QStringLiteral("background")).isValid());
        }
        QCOMPARE(darkCount, 4);
        QCOMPARE(lightCount, 4);
        QCOMPARE(NotePeek::SyntaxThemeCatalog::theme(QStringLiteral("auto"), true).id,
                 QStringLiteral("notepeek-dark-modern"));
        QCOMPARE(NotePeek::SyntaxThemeCatalog::theme(QStringLiteral("auto"), false).id,
                 QStringLiteral("notepeek-light-modern"));
    }

    void rejectsIncompleteSyntaxThemeJson()
    {
        QString error;
        const NotePeek::SyntaxTheme theme = NotePeek::SyntaxThemeCatalog::fromJson(
            QByteArrayLiteral("{\"id\":\"broken\",\"name\":\"Broken\",\"appearance\":\"dark\"}"),
            &error);
        QVERIFY(!theme.isValid());
        QVERIFY(!error.isEmpty());
    }

    void appliesSelectedSyntaxThemeToAnEditor()
    {
        AppPreferences &preferences = AppPreferences::instance();
        const QString previousTheme = preferences.syntaxThemeId();
        preferences.setSyntaxThemeId(QStringLiteral("notepeek-midnight-blue"));

        EditorManager manager;
        CustomEditor *editor = manager.createEditor();
        manager.updateEditorSyntax(editor, QStringLiteral("sample.py"));
        auto *python = qobject_cast<QsciLexerPython *>(editor->lexer());
        QVERIFY(python);
        const NotePeek::SyntaxTheme midnight = NotePeek::SyntaxThemeCatalog::theme(
            QStringLiteral("notepeek-midnight-blue"), true);
        QCOMPARE(python->color(QsciLexerPython::FunctionMethodName),
                 midnight.syntaxColors.value(NotePeek::SyntaxRole::Function));
        QCOMPARE(python->paper(QsciLexerPython::Default),
                 midnight.editorColor(QStringLiteral("background")));

        preferences.setSyntaxThemeId(QStringLiteral("notepeek-light-modern"));
        manager.reapplyThemeToAllEditors();
        const NotePeek::SyntaxTheme light = NotePeek::SyntaxThemeCatalog::theme(
            QStringLiteral("notepeek-light-modern"), false);
        QCOMPARE(python->color(QsciLexerPython::FunctionMethodName),
                 light.syntaxColors.value(NotePeek::SyntaxRole::Function));

        preferences.setSyntaxThemeId(previousTheme);
    }

    void exposesAllSyntaxThemesInPreferences()
    {
        PreferencesDialog dialog;
        QComboBox *themeCombo = dialog.findChild<QComboBox *>(QStringLiteral("syntaxThemeCombo"));
        QVERIFY(themeCombo);
        QCOMPARE(themeCombo->count(), 9); // automatic + eight built-ins
        QCOMPARE(themeCombo->itemData(0).toString(), QStringLiteral("auto"));
    }

    void classifiesRepresentativeSemanticNames()
    {
        NotePeek::BasicSemanticTokenProvider provider;
        NotePeek::SemanticDocument document;
        document.languageId = QStringLiteral("C++");
        document.text = QStringLiteral(
            "Widget buildWidget(int count) { object.title = makeTitle(count); }\n"
            "// IgnoredCall(CommentType)\n"
            "const char *text = \"IgnoredString(Call)\";\n");
        const QVector<NotePeek::SemanticToken> tokens = provider.tokensForDocument(document);

        bool foundType = false;
        bool foundFunction = false;
        bool foundProperty = false;
        for (const NotePeek::SemanticToken &token : tokens)
        {
            const QString line = document.text.split(QLatin1Char('\n')).value(token.line);
            const QString value = line.mid(token.utf16Column, token.utf16Length);
            foundType |= value == QLatin1String("Widget") && token.role == NotePeek::SyntaxRole::Type;
            foundFunction |= value == QLatin1String("makeTitle") && token.role == NotePeek::SyntaxRole::Function;
            foundProperty |= value == QLatin1String("title") && token.role == NotePeek::SyntaxRole::Property;
            QVERIFY(value != QLatin1String("IgnoredCall"));
            QVERIFY(value != QLatin1String("IgnoredString"));
        }
        QVERIFY(foundType);
        QVERIFY(foundFunction);
        QVERIFY(foundProperty);
    }

    void treatsPythonDoubleSlashAsDivisionRatherThanComment()
    {
        NotePeek::BasicSemanticTokenProvider provider;
        NotePeek::SemanticDocument document;
        document.languageId = QStringLiteral("Python");
        document.text = QStringLiteral(
            "total = countItems(rows) // scaleFactor(columns)\n"
            "# hiddenCall(HiddenType)\n");

        const QStringList lines = document.text.split(QLatin1Char('\n'));
        bool foundBeforeSlashes = false;
        bool foundAfterSlashes = false;
        for (const NotePeek::SemanticToken &token : provider.tokensForDocument(document))
        {
            const QString value = lines.value(token.line).mid(token.utf16Column, token.utf16Length);
            foundBeforeSlashes |= value == QLatin1String("countItems");
            // `//` is floor division in Python: everything after it is code and
            // must still be classified.
            foundAfterSlashes |= value == QLatin1String("scaleFactor");
            QVERIFY(value != QLatin1String("hiddenCall"));
            QVERIFY(value != QLatin1String("HiddenType"));
        }
        QVERIFY(foundBeforeSlashes);
        QVERIFY(foundAfterSlashes);
    }

    void masksPythonTripleQuotedStringsAcrossLines()
    {
        NotePeek::BasicSemanticTokenProvider provider;
        NotePeek::SemanticDocument document;
        document.languageId = QStringLiteral("Python");
        document.text = QStringLiteral(
            "\"\"\"docComment(DocType)\n"
            "stillInsideDoc(DocType)\n"
            "\"\"\"\n"
            "realCall(RealType)\n");

        const QStringList lines = document.text.split(QLatin1Char('\n'));
        bool foundRealCall = false;
        for (const NotePeek::SemanticToken &token : provider.tokensForDocument(document))
        {
            const QString value = lines.value(token.line).mid(token.utf16Column, token.utf16Length);
            foundRealCall |= value == QLatin1String("realCall");
            QVERIFY(value != QLatin1String("docComment"));
            QVERIFY(value != QLatin1String("stillInsideDoc"));
        }
        QVERIFY(foundRealCall);
    }

    void honorsEntryStateWhenAWindowStartsInsideACommentOrString()
    {
        NotePeek::BasicSemanticTokenProvider provider;
        NotePeek::SemanticDocument document;
        document.languageId = QStringLiteral("C++");
        document.firstLine = 500;
        document.text = QStringLiteral(
            "stillCommented(CommentedType)\n"
            "*/ realCall(RealType)\n");

        // Default entry state: the window looks like plain code.
        bool sawCommentedName = false;
        for (const NotePeek::SemanticToken &token : provider.analyze(document).tokens)
        {
            const QString value = document.text.split(QLatin1Char('\n'))
                                      .value(token.line - document.firstLine)
                                      .mid(token.utf16Column, token.utf16Length);
            sawCommentedName |= value == QLatin1String("stillCommented");
        }
        QVERIFY(sawCommentedName);

        // Told that a block comment is open, the same window must suppress
        // everything up to the closing delimiter.
        document.entryState = NotePeek::SemanticEntryState::BlockComment;
        const NotePeek::SemanticAnalysisResult guarded = provider.analyze(document);
        bool foundRealCall = false;
        for (const NotePeek::SemanticToken &token : guarded.tokens)
        {
            const QString value = document.text.split(QLatin1Char('\n'))
                                      .value(token.line - document.firstLine)
                                      .mid(token.utf16Column, token.utf16Length);
            foundRealCall |= value == QLatin1String("realCall");
            QVERIFY(value != QLatin1String("stillCommented"));
            QVERIFY(value != QLatin1String("CommentedType"));
        }
        QVERIFY(foundRealCall);
        QCOMPARE(guarded.exitState, NotePeek::SemanticEntryState::Code);
    }

    void resolvesOverlappingPatternsByExplicitPriority()
    {
        NotePeek::BasicSemanticTokenProvider provider;
        NotePeek::SemanticDocument document;
        document.languageId = QStringLiteral("C++");
        document.text = QStringLiteral("MAX_ITEMS = object.member + Helper(value);\n");

        QMap<QString, NotePeek::SyntaxRole> roles;
        for (const NotePeek::SemanticToken &token : provider.tokensForDocument(document))
            roles.insert(document.text.mid(token.utf16Column, token.utf16Length), token.role);

        // An all-caps name matches both the macro and the type pattern; the
        // macro rule is more specific and wins regardless of rule order.
        QCOMPARE(roles.value(QStringLiteral("MAX_ITEMS")), NotePeek::SyntaxRole::Builtin);
        QCOMPARE(roles.value(QStringLiteral("member")), NotePeek::SyntaxRole::Property);
        QCOMPARE(roles.value(QStringLiteral("Helper")), NotePeek::SyntaxRole::Type);
    }

    void semanticAnalysisHonorsViewportOffsetsCancellationAndBudgets()
    {
        NotePeek::BasicSemanticTokenProvider provider;
        NotePeek::SemanticDocument document;
        document.languageId = QStringLiteral("C++");
        document.firstLine = 400;
        document.revision = 17;
        document.text = QStringLiteral("Widget buildWidget() { return object.title; }\n");

        const NotePeek::SemanticAnalysisResult offsetResult = provider.analyze(document);
        QCOMPARE(offsetResult.revision, quint64(17));
        QCOMPARE(offsetResult.firstLine, 400);
        QCOMPARE(offsetResult.lastLine, 401);
        QVERIFY(!offsetResult.tokens.isEmpty());
        for (const NotePeek::SemanticToken &token : offsetResult.tokens)
            QVERIFY(token.line >= 400);

        NotePeek::SemanticAnalysisControl canceledControl;
        canceledControl.cancellation = std::make_shared<std::atomic_bool>(true);
        const NotePeek::SemanticAnalysisResult canceled = provider.analyze(document, canceledControl);
        QVERIFY(canceled.canceled);
        QVERIFY(canceled.tokens.isEmpty());

        NotePeek::SemanticAnalysisControl boundedControl;
        boundedControl.budgetNanoseconds = 1;
        const NotePeek::SemanticAnalysisResult bounded = provider.analyze(document, boundedControl);
        QVERIFY(bounded.budgetExceeded);
        QVERIFY(bounded.elapsedNanoseconds > 0);
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

    // The Tools entry has to be offered only for files a formatter handles, and
    // one click must be one undo step.
    void formatCodeActionReformatsSupportedFilesOnly()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString jsonPath = dir.filePath(QStringLiteral("data.json"));
        const QByteArray original = QByteArrayLiteral("{\"b\":1,\"a\":[2,3]}\n");
        writeFile(jsonPath, original);
        const QString textPath = dir.filePath(QStringLiteral("notes.txt"));
        writeFile(textPath, QByteArrayLiteral("plain text\n"));

        MainWindow window(nullptr, { jsonPath });
        window.show();

        auto *action = window.findChild<QAction *>(QStringLiteral("action_formatCode"));
        QVERIFY(action);
        QTRY_VERIFY(action->isEnabled());

        const auto editorFor = [&window](const QString &path) -> CustomEditor * {
            for (CustomEditor *candidate : window.findChildren<CustomEditor *>())
            {
                if (QFileInfo(candidate->filePath()) == QFileInfo(path))
                {
                    return candidate;
                }
            }
            return nullptr;
        };

        CustomEditor *editor = nullptr;
        QTRY_VERIFY((editor = editorFor(jsonPath)) != nullptr);
        QTRY_COMPARE(editor->text(), QString::fromUtf8(original));

        action->trigger();
        QTRY_VERIFY(editor->text().startsWith(QStringLiteral("{\n")));
        QVERIFY(editor->text().contains(QStringLiteral("\"b\": 1")));
        // Streaming the source keeps member order; a QJsonDocument round trip
        // would have sorted "a" ahead of "b".
        QVERIFY(editor->text().indexOf(QStringLiteral("\"b\"")) < editor->text().indexOf(QStringLiteral("\"a\"")));
        // The whole-document replacement must not leave the document selected.
        QVERIFY(!editor->hasSelectedText());

        editor->undo();
        QCOMPARE(editor->text(), QString::fromUtf8(original));

        window.openFileByArg(textPath);
        QTRY_VERIFY(editorFor(textPath) != nullptr);
        QTRY_VERIFY(!action->isEnabled());
    }

    // A name column wider than the viewport is clipped by the viewport instead
    // of being elided, so the column has to follow the dock's width.
    void fileTreeNameColumnTracksTheViewport()
    {
        QMainWindow window;
        FileExplorerDock dock(&window);
        window.addDockWidget(Qt::LeftDockWidgetArea, dock.dockWidget());
        window.resize(700, 400);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto *tree = qobject_cast<QTreeView *>(dock.dockWidget()->widget());
        QVERIFY(tree);
        QCOMPARE(tree->textElideMode(), Qt::ElideRight);
        QCOMPARE(tree->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
        QTRY_COMPARE(tree->header()->sectionSize(0), tree->viewport()->width());

        dock.dockWidget()->resize(160, 400);
        QTRY_COMPARE(tree->header()->sectionSize(0), tree->viewport()->width());
    }

    void fileTreeExposesTheFullNameAsTooltip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString longName =
            QStringLiteral("a-very-long-note-name-that-cannot-fit-in-the-file-tree-dock.md");
        writeFile(dir.filePath(longName), QByteArrayLiteral("# note\n"));

        QFileSystemModel model;
        model.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
        FileTypeFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        model.setRootPath(dir.path());
        proxy.setVisibleRootDirectory(dir.path());

        const QString filePath = dir.filePath(longName);
        QModelIndex proxyIndex;
        QTRY_VERIFY([&]() {
            const QModelIndex sourceIndex = model.index(filePath);
            proxyIndex = sourceIndex.isValid() ? proxy.mapFromSource(sourceIndex) : QModelIndex();
            return proxyIndex.isValid();
        }());
        QCOMPARE(proxy.data(proxyIndex, Qt::ToolTipRole).toString(), longName);
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
