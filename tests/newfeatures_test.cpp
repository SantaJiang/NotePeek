#include <QMenu>
#include <QComboBox>
#include <QDockWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
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
#include "widgets/searchresultsdock.h"
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
