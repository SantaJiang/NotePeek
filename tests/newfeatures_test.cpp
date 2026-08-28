#include <QAction>
#include <QCheckBox>
#include <QMenu>
#include <QComboBox>
#include <QDockWidget>
#include <QDir>
#include <QMainWindow>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QToolButton>
#include <QSignalSpy>
#include <QScrollBar>
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QScopeGuard>
#include <QStyleHints>
#endif
#include <QStackedWidget>
#include <QStringList>
#include <QTimer>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>

#include "apppreferences.h"
#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/documentformat.h"
#include "editor/documentpath.h"
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
#include "versioncontrol/gitexecutable.h"
#include "versioncontrol/gitrepositoryservice.h"
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

bool runGit(const QString &gitExecutable,
            const QString &workingDirectory,
            const QStringList &arguments)
{
    QProcess process;
    process.setProgram(gitExecutable);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    process.start();
    return process.waitForStarted(3000) && process.waitForFinished(10000)
           && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

class GitIntegrationPreferenceGuard
{
public:
    explicit GitIntegrationPreferenceGuard(bool enabled)
        : m_original(AppPreferences::instance().gitIntegrationEnabled())
    {
        AppPreferences::instance().setGitIntegrationEnabled(enabled);
    }

    ~GitIntegrationPreferenceGuard()
    {
        AppPreferences::instance().setGitIntegrationEnabled(m_original);
        AppPreferences::instance().save();
    }

private:
    bool m_original;
};

long scintillaColorValue(const QColor &color)
{
    return (color.blue() << 16) | (color.green() << 8) | color.red();
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
        QVERIFY(AppPreferences::instance().gitIntegrationEnabled());
        MainWindow blankWindow;
        blankWindow.show();
        QVERIFY(blankWindow.isVisible());
        QVERIFY(blankWindow.findChild<GitRepositoryService *>());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("opened.txt"));
        writeFile(filePath, QByteArrayLiteral("hello\n"));

        MainWindow fileWindow(nullptr, { filePath });
        fileWindow.show();
        QVERIFY(fileWindow.isVisible());
        QVERIFY(fileWindow.findChild<QMenu *>() != nullptr);
    }

    void saveAllActionSavesTheOpenDocument()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("save-all.txt"));
        writeFile(filePath, QByteArrayLiteral("before"));

        MainWindow window(nullptr, { filePath });
        window.show();
        CustomEditor *editor = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT(([&window, &editor, &filePath]() {
            for (EditorPane *pane : window.findChildren<EditorPane *>())
            {
                for (int index = 0; index < pane->count(); ++index)
                {
                    CustomEditor *candidate = pane->editorAt(index);
                    if (candidate && DocumentPath::equivalent(candidate->filePath(), filePath)
                        && !candidate->isReadOnly())
                    {
                        editor = candidate;
                        return true;
                    }
                }
            }
            return false;
        })(), 3000);
        QVERIFY(editor);
        editor->append(QStringLiteral("-after"));

        QAction *saveAllAction = window.findChild<QAction *>(QStringLiteral("action_saveAll"));
        QVERIFY(saveAllAction);
        saveAllAction->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(QFileInfo(filePath).size(), qint64(12), 3000);

        QFile saved(filePath);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(saved.readAll(), QByteArrayLiteral("before-after"));
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

    void fileTreeThemePaletteSurvivesLightToDarkRepolish()
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QStyleHints *styleHints = QGuiApplication::styleHints();
        const Qt::ColorScheme originalScheme = styleHints->colorScheme();
        const auto restoreScheme = qScopeGuard([styleHints, originalScheme]() {
            styleHints->setColorScheme(originalScheme);
            QCoreApplication::processEvents();
        });

        styleHints->setColorScheme(Qt::ColorScheme::Light);
        QCoreApplication::processEvents();
        const bool canOverrideScheme =
            styleHints->colorScheme() == Qt::ColorScheme::Light;
#endif

        QMainWindow window;
        FileExplorerDock dock(&window);
        auto *tree = qobject_cast<QTreeView *>(dock.dockWidget()->widget());
        QVERIFY(tree);

        const auto verifyPalette = [tree]() {
            const QPalette palette = tree->palette();
            for (const QPalette::ColorGroup group : {
                     QPalette::Active, QPalette::Inactive, QPalette::Disabled })
            {
                QCOMPARE(palette.color(group, QPalette::Base),
                         ThemeColors::FileTree::background());
                QCOMPARE(palette.color(group, QPalette::Window),
                         ThemeColors::FileTree::background());
                QCOMPARE(palette.color(group, QPalette::Text),
                         ThemeColors::FileTree::text());
                QCOMPARE(palette.color(group, QPalette::Highlight),
                         ThemeColors::FileTree::selectionBg());
                QCOMPARE(palette.color(group, QPalette::HighlightedText),
                         ThemeColors::FileTree::selectionText());
            }

            const QString styleSheet = tree->styleSheet();
            QVERIFY(styleSheet.contains(
                ThemeColors::FileTree::background().name(QColor::HexRgb)));
            QVERIFY(styleSheet.contains(
                ThemeColors::FileTree::text().name(QColor::HexRgb)));
            QVERIFY(styleSheet.contains(
                ThemeColors::FileTree::hoverBg().name(QColor::HexRgb)));
            QVERIFY(styleSheet.contains(
                ThemeColors::FileTree::selectionBg().name(QColor::HexRgb)));
            QVERIFY(styleSheet.contains(
                ThemeColors::FileTree::selectionText().name(QColor::HexRgb)));
            QVERIFY(styleSheet.contains(QStringLiteral("QTreeView::item:hover")));
            QVERIFY(styleSheet.contains(QStringLiteral("QTreeView::item:selected")));
        };

        verifyPalette();

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (!canOverrideScheme)
            return;

        styleHints->setColorScheme(Qt::ColorScheme::Dark);
        QCoreApplication::processEvents();
        QCOMPARE(styleHints->colorScheme(), Qt::ColorScheme::Dark);
        dock.reapplyTheme();
        verifyPalette();
#endif
    }

    void usesRequestedChromeBackgroundColors()
    {
        const ThemeColors::RoleColors chrome =
            ThemeColors::roleColors(ThemeColors::ColorRole::ChromeBackground);
        const ThemeColors::RoleColors fileTree =
            ThemeColors::roleColors(ThemeColors::ColorRole::FileTreeBackground);

        QCOMPARE(chrome.light, qRgb(250, 250, 250));
        QCOMPARE(chrome.dark, qRgb(51, 51, 51));
        QCOMPARE(fileTree.light, qRgb(250, 250, 250));
        QCOMPARE(fileTree.dark, qRgb(38, 38, 38));

        FindReplaceWidget findReplace;
        QVERIFY(findReplace.styleSheet().contains(
            ThemeColors::EditorTopBar::background().name(QColor::HexRgb)));
        const bool systemDark = ThemeColors::isDarkMode();
        const NotePeek::SyntaxTheme syntaxTheme = NotePeek::SyntaxThemeCatalog::theme(
            AppPreferences::instance().currentSyntaxThemeId(systemDark), systemDark);
        const QString syntaxBackground = syntaxTheme.editorColor(
            QStringLiteral("background"), ThemeColors::Editor::background())
                                             .name(QColor::HexRgb);
        QVERIFY(findReplace.styleSheet().contains(syntaxBackground));
    }

    void statusBarUsesCompactRightAlignedDocumentInfo()
    {
        MainWindow window;
        window.resize(900, 600);
        window.show();
        QApplication::processEvents();

        auto *statusBar = window.findChild<QsciEditorInfoStatusBar *>();
        auto *editor = window.findChild<CustomEditor *>();
        auto *position = statusBar
            ? statusBar->findChild<QLabel *>(QStringLiteral("statusDocumentPosition"))
            : nullptr;
        auto *encoding = statusBar
            ? statusBar->findChild<QLabel *>(QStringLiteral("statusEncoding"))
            : nullptr;
        auto *lineEnding = statusBar
            ? statusBar->findChild<QLabel *>(QStringLiteral("statusLineEnding"))
            : nullptr;
        auto *documentType = statusBar
            ? statusBar->findChild<QLabel *>(QStringLiteral("statusDocumentType"))
            : nullptr;
        const QStringList spacingObjectNames = {
            QStringLiteral("statusSpacingAfterPosition"),
            QStringLiteral("statusSpacingAfterEncoding"),
            QStringLiteral("statusSpacingAfterLineEnding"),
            QStringLiteral("statusRightEdgeSpacing")
        };
        QVERIFY(statusBar);
        QVERIFY(editor);
        QVERIFY(position);
        QVERIFY(encoding);
        QVERIFY(lineEnding);
        QVERIFY(documentType);

        editor->setText(QStringLiteral("alpha\nbeta"));
        editor->setCursorPosition(1, 2);
        statusBar->refresh(editor);
        QCOMPARE(position->text(), QStringLiteral("Line 2, Column 3"));

        editor->setSelection(0, 1, 0, 4);
        statusBar->refresh(editor);
        QCOMPARE(position->text(), QStringLiteral("Line 1, Column 5 (Selected 3)"));

        QVERIFY(position->geometry().left() < encoding->geometry().left());
        QVERIFY(encoding->geometry().left() < lineEnding->geometry().left());
        QVERIFY(lineEnding->geometry().left() < documentType->geometry().left());
        for (const QString &objectName : spacingObjectNames)
        {
            auto *spacing = statusBar->findChild<QWidget *>(objectName);
            QVERIFY(spacing);
            QCOMPARE(spacing->width(), 15);
        }
        statusBar->setGitStatus(QStringLiteral("main"));
        auto *gitIcon = statusBar->findChild<QLabel *>(
            QStringLiteral("statusGitBranchIcon"));
        QVERIFY(gitIcon);
        auto *rightEdgeSpacing = statusBar->findChild<QWidget *>(
            QStringLiteral("statusRightEdgeSpacing"));
        QVERIFY(rightEdgeSpacing);
        QVERIFY(!statusBar->isSizeGripEnabled());
        const int initialLeftEdgeSpacing =
            gitIcon->mapTo(statusBar, QPoint(0, 0)).x();
        const int initialRightEdgeSpacing =
            statusBar->width() - documentType->geometry().right() - 1;
        QVERIFY(initialLeftEdgeSpacing >= 15);
        QVERIFY(initialRightEdgeSpacing >= 15);

        window.resize(1400, 800);
        QApplication::processEvents();
        QCOMPARE(gitIcon->mapTo(statusBar, QPoint(0, 0)).x(),
                 initialLeftEdgeSpacing);
        QCOMPARE(statusBar->width() - documentType->geometry().right() - 1,
                 initialRightEdgeSpacing);
        QCOMPARE(rightEdgeSpacing->width(), 15);

        window.showMaximized();
        QApplication::processEvents();
        QCOMPARE(gitIcon->mapTo(statusBar, QPoint(0, 0)).x(),
                 initialLeftEdgeSpacing);
        QCOMPARE(statusBar->width() - documentType->geometry().right() - 1,
                 initialRightEdgeSpacing);
        QVERIFY(statusBar->styleSheet().contains(
            ThemeColors::EditorTopBar::text().name(QColor::HexRgb)));
    }

    void statusBarShowsClickableGitSummary()
    {
        QMainWindow window;
        QsciEditorInfoStatusBar statusBar(&window);
        QWidget *gitContainer = statusBar.findChild<QWidget *>(
            QStringLiteral("statusGitContainer"));
        QWidget *gitLeftPadding = statusBar.findChild<QWidget *>(
            QStringLiteral("statusGitLeftPadding"));
        QLabel *gitIcon = statusBar.findChild<QLabel *>(
            QStringLiteral("statusGitBranchIcon"));
        QLabel *gitLabel = statusBar.findChild<QLabel *>(
            QStringLiteral("statusGitRepository"));
        QVERIFY(gitContainer);
        QVERIFY(gitLeftPadding);
        QVERIFY(gitIcon);
        QVERIFY(gitLabel);
        QVERIFY(gitContainer->isHidden());
        QCOMPARE(gitIcon->text(), QString(QChar(0xf126)));

        QSignalSpy refreshSpy(&statusBar,
                              &QsciEditorInfoStatusBar::gitRefreshRequested);
        statusBar.show();
        statusBar.setGitStatus(QStringLiteral("main  •2"),
                               QStringLiteral("Click to refresh"));
        QVERIFY(!gitContainer->isHidden());
        QCOMPARE(gitLabel->text(), QStringLiteral("main  •2"));
        QTest::mouseClick(gitIcon, Qt::LeftButton);
        QCOMPARE(refreshSpy.count(), 1);
        QTest::mouseClick(gitLabel, Qt::LeftButton);
        QCOMPARE(refreshSpy.count(), 2);

        statusBar.setGitStatus(QString());
        QVERIFY(gitContainer->isHidden());
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
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("build")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("node_modules/package")));
        writeFile(root.filePath(QStringLiteral("alpha.txt")),
                  QByteArrayLiteral("needle here\nno match\nanother needle\n"));
        writeFile(root.filePath(QStringLiteral("sub/beta.cpp")),
                  QByteArrayLiteral("int needle = 0;\n"));
        writeFile(root.filePath(QStringLiteral("binary.bin")),
                  QByteArray("nee\x00dle", 7));
        writeFile(root.filePath(QStringLiteral("build/generated.txt")),
                  QByteArrayLiteral("needle must be ignored\n"));
        writeFile(root.filePath(QStringLiteral("node_modules/package/index.js")),
                  QByteArrayLiteral("needle must be ignored\n"));

        FileSearcher searcher;
        QSignalSpy finished(&searcher, &FileSearcher::searchFinished);
        searcher.start(root.path(), QStringLiteral("needle"), false, false);
        QTRY_COMPARE(finished.count(), 1);

        const auto outcome = finished.takeFirst().first().value<FileSearchOutcome>();
        QCOMPARE(outcome.summary.matchCount, 3);
        QCOMPARE(outcome.summary.scannedFileCount, 2);
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

    void searchRespectsGitIgnore()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        writeFile(root.filePath(QStringLiteral(".gitignore")),
                  QByteArrayLiteral("ignored-*\n!keep-generated.txt\n"));
        writeFile(root.filePath(QStringLiteral("keep.txt")),
                  QByteArrayLiteral("needle\n"));
        writeFile(root.filePath(QStringLiteral("keep-generated.txt")),
                  QByteArrayLiteral("needle\n"));
        writeFile(root.filePath(QStringLiteral("ignored-generated.txt")),
                  QByteArrayLiteral("needle\n"));
        writeFile(root.filePath(QStringLiteral("explicit.log")),
                  QByteArrayLiteral("needle\n"));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("ignored-directory")));
        writeFile(root.filePath(QStringLiteral("ignored-directory/nested.txt")),
                  QByteArrayLiteral("needle\n"));

        FileSearcher searcher;
        QSignalSpy finished(&searcher, &FileSearcher::searchFinished);
        searcher.start(root.path(), QStringLiteral("needle"), true, false);
        QTRY_COMPARE(finished.count(), 1);

        const auto outcome = finished.takeFirst().first().value<FileSearchOutcome>();
        QCOMPARE(outcome.summary.matchCount, 3);
        QStringList matchedFiles;
        for (const FileSearchMatch &match : outcome.matches)
            matchedFiles.append(QFileInfo(match.filePath).fileName());
        matchedFiles.sort();
        QCOMPARE(matchedFiles,
                 QStringList({QStringLiteral("explicit.log"),
                              QStringLiteral("keep-generated.txt"),
                              QStringLiteral("keep.txt")}));
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
        auto *excludeEdit = dockWidget->findChild<QLineEdit *>(QStringLiteral("searchResultsExcludeEdit"));
        auto *caseButton = dockWidget->findChild<QToolButton *>(QStringLiteral("searchResultsCaseButton"));
        auto *wholeWordButton = dockWidget->findChild<QToolButton *>(QStringLiteral("searchResultsWholeWordButton"));
        auto *searchButton = dockWidget->findChild<QToolButton *>(QStringLiteral("searchResultsSearchButton"));
        auto *resultsTree = dockWidget->findChild<QTreeWidget *>();
        QVERIFY(titleBar);
        QVERIFY(closeButton);
        QVERIFY(patternEdit);
        QVERIFY(patternEdit->placeholderText().isEmpty());
        QVERIFY(!excludeEdit);
        QVERIFY(caseButton);
        QVERIFY(wholeWordButton);
        QVERIFY(searchButton);
        QVERIFY(resultsTree);
        QVERIFY(!titleBar->styleSheet().isEmpty());
        QVERIFY(!searchButton->styleSheet().isEmpty());
        const bool systemDark = ThemeColors::isDarkMode();
        const NotePeek::SyntaxTheme syntaxTheme = NotePeek::SyntaxThemeCatalog::theme(
            AppPreferences::instance().currentSyntaxThemeId(systemDark), systemDark);
        const QString syntaxBackground = syntaxTheme.editorColor(
            QStringLiteral("background"), ThemeColors::Editor::background())
                                             .name(QColor::HexRgb);
        QVERIFY(dockWidget->widget()->styleSheet().contains(syntaxBackground));
        QVERIFY(resultsTree->styleSheet().contains(syntaxBackground));
        QCOMPARE(caseButton->height(), patternEdit->sizeHint().height());
        QCOMPARE(wholeWordButton->height(), patternEdit->sizeHint().height());

        auto *wrapButton = findReplace->findChild<QToolButton *>(QStringLiteral("buttonWrapAround"));
        auto *findCaseButton = findReplace->findChild<QToolButton *>(QStringLiteral("buttonMatchCase"));
        auto *findWholeWordButton = findReplace->findChild<QToolButton *>(QStringLiteral("buttonMatchWholeWord"));
        auto *backwardsButton = findReplace->findChild<QToolButton *>(QStringLiteral("buttonBackwardsDirection"));
        QVERIFY(wrapButton);
        QVERIFY(findCaseButton);
        QVERIFY(findWholeWordButton);
        QVERIFY(backwardsButton);
        QVERIFY(wrapButton->isCheckable());
        QVERIFY(wrapButton->isChecked());
        QVERIFY(findCaseButton->isCheckable());
        QVERIFY(findWholeWordButton->isCheckable());
        QCOMPARE(wrapButton->text(), QStringLiteral("↻"));
        QCOMPARE(findCaseButton->text(), QStringLiteral("Aa"));
        QCOMPARE(findWholeWordButton->text(), QStringLiteral("W"));
        QVERIFY(backwardsButton->isCheckable());
        QCOMPARE(backwardsButton->text(), QStringLiteral("←"));
        const QList<QToolButton *> compactFindOptions = {
            wrapButton, findCaseButton, findWholeWordButton, backwardsButton
        };
        for (QToolButton *button : compactFindOptions)
        {
            QCOMPARE(button->minimumSize(), QSize(22, 22));
            QCOMPARE(button->maximumSize(), QSize(22, 22));
        }

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

    void identifiesLanguagesFromHighConfidenceContent()
    {
        QCOMPARE(LexerFactory::languageFromContent(
                     QByteArrayLiteral("#!/usr/bin/env python3\nprint('hello')\n")),
                 LexerFactory::Python);
        QCOMPARE(LexerFactory::languageFromContent(
                     QByteArrayLiteral("#!/bin/zsh\necho hello\n")),
                 LexerFactory::SHELL);
        QCOMPARE(LexerFactory::languageFromContent(
                     QByteArrayLiteral("  <?xml version=\"1.0\"?><root/>")),
                 LexerFactory::XML);
        QCOMPARE(LexerFactory::languageFromContent(
                     QByteArrayLiteral("{\"name\": \"NotePeek\", \"enabled\": true}")),
                 LexerFactory::JSON);
        QCOMPARE(LexerFactory::languageFromContent(
                     QByteArrayLiteral("ordinary prose with { punctuation }")),
                 LexerFactory::Unknown);
        QCOMPARE(LexerFactory::languageFromContent(
                     QByteArrayLiteral("<htmlish is not an HTML root tag>")),
                 LexerFactory::Unknown);
        QCOMPARE(LexerFactory::languageFromContent(
                     QByteArrayLiteral("<!doctype htmlish>")),
                 LexerFactory::Unknown);
    }

    void appliesContentDetectedLexerAfterAsynchronousOpen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("extensionless-script"));
        writeFile(filePath, QByteArrayLiteral("#!/usr/bin/env python3\nprint('hello')\n"));

        EditorPane pane;
        DocumentController controller(&pane, false);
        controller.openFile(filePath);
        QTRY_VERIFY_WITH_TIMEOUT(controller.currentEditor()
                                     && !controller.currentEditor()->isReadOnly(),
                                 3000);
        QVERIFY(qobject_cast<QsciLexerPython *>(controller.currentEditor()->lexer()));
        QCOMPARE(controller.currentEditor()->property("notepeekLexerLanguage").toInt(),
                 int(LexerFactory::Python));
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
        QCOMPARE(themes.size(), 10);
        int darkCount = 0;
        int lightCount = 0;
        QSet<QString> ids;
        for (const NotePeek::SyntaxTheme &theme : themes)
        {
            QVERIFY2(theme.isValid(), qPrintable(theme.id));
            QVERIFY(!ids.contains(theme.id));
            ids.insert(theme.id);
            theme.dark ? ++darkCount : ++lightCount;
            QVERIFY(!theme.sourceRepository.isEmpty());
            QVERIFY(!theme.sourceRevision.isEmpty());
            QVERIFY(!theme.sourceLicense.isEmpty());
            QVERIFY(!theme.name.contains(QStringLiteral("VS Code")));
            QVERIFY(!theme.name.contains(QStringLiteral("Qt Creator")));
            QCOMPARE(theme.syntaxColors.size(), 22);
            QVERIFY(theme.editorColor(QStringLiteral("background")).isValid());
        }
        QCOMPARE(darkCount, 5);
        QCOMPARE(lightCount, 5);
        const QStringList ayuIds = {
            QStringLiteral("upstream-ayu-dark"),
            QStringLiteral("upstream-ayu-light")
        };
        for (const QString &ayuId : ayuIds)
            QVERIFY2(ids.contains(ayuId), qPrintable(ayuId));
        QCOMPARE(NotePeek::SyntaxThemeCatalog::theme(QStringLiteral("auto"), true).id,
                 QStringLiteral("upstream-vscode-dark-2026"));
        QCOMPARE(NotePeek::SyntaxThemeCatalog::theme(QStringLiteral("auto"), false).id,
                 QStringLiteral("upstream-vscode-light-2026"));
    }

    void loadsSyntaxThemesFromADirectory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile builtIn(QStringLiteral(":/notepeek/themes/ayu-dark.json"));
        QVERIFY(builtIn.open(QFile::ReadOnly));
        writeFile(directory.filePath(QStringLiteral("custom.json")), builtIn.readAll());
        writeFile(directory.filePath(QStringLiteral("invalid.json")),
                  QByteArrayLiteral("{\"id\":\"invalid\"}"));
        writeFile(directory.filePath(QStringLiteral("ignored.txt")),
                  QByteArrayLiteral("not a theme"));

        const QVector<NotePeek::SyntaxTheme> themes =
            NotePeek::SyntaxThemeCatalog::loadFromDirectory(directory.path());
        QCOMPARE(themes.size(), 1);
        QCOMPARE(themes.first().id, QStringLiteral("upstream-ayu-dark"));
        QCOMPARE(themes.first().filePath, directory.filePath(QStringLiteral("custom.json")));
        QVERIFY(!themes.first().builtIn);
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
        const QString previousDarkTheme = preferences.darkSyntaxThemeId();
        const QString previousLightTheme = preferences.lightSyntaxThemeId();
        const bool systemDark = ThemeColors::isDarkMode();
        preferences.setDarkSyntaxThemeId(QStringLiteral("upstream-rose-pine"));
        preferences.setLightSyntaxThemeId(QStringLiteral("upstream-ayu-light"));
        // Syntax highlighting always tracks the system appearance: the active
        // theme is picked from the dark/light pair by the systemDark flag.
        QCOMPARE(preferences.currentSyntaxThemeId(true),
                 QStringLiteral("upstream-rose-pine"));
        QCOMPARE(preferences.currentSyntaxThemeId(false),
                 QStringLiteral("upstream-ayu-light"));

        EditorManager manager;
        CustomEditor *editor = manager.createEditor();
        manager.updateEditorSyntax(editor, QStringLiteral("sample.py"));
        auto *python = qobject_cast<QsciLexerPython *>(editor->lexer());
        QVERIFY(python);
        const QString activeThemeId = preferences.currentSyntaxThemeId(systemDark);
        const NotePeek::SyntaxTheme activeTheme =
            NotePeek::SyntaxThemeCatalog::theme(activeThemeId, systemDark);
        QCOMPARE(python->color(QsciLexerPython::FunctionMethodName),
                 activeTheme.syntaxColors.value(NotePeek::SyntaxRole::Function));
        QCOMPARE(python->paper(QsciLexerPython::Default),
                 activeTheme.editorColor(QStringLiteral("background")));
        const long marginBackground = editor->SendScintilla(
            QsciScintillaBase::SCI_STYLEGETBACK, QsciScintillaBase::STYLE_LINENUMBER);
        QCOMPARE(marginBackground,
                 scintillaColorValue(activeTheme.editorColor(QStringLiteral("background"))));

        // Switching the slot for the current appearance re-applies the new
        // theme to open editors.
        const QString reappliedId = systemDark
            ? QStringLiteral("upstream-ayu-dark")
            : QStringLiteral("upstream-vscode-light-2026");
        if (systemDark)
            preferences.setDarkSyntaxThemeId(reappliedId);
        else
            preferences.setLightSyntaxThemeId(reappliedId);
        manager.reapplyThemeToAllEditors();
        const NotePeek::SyntaxTheme reappliedTheme =
            NotePeek::SyntaxThemeCatalog::theme(
                preferences.currentSyntaxThemeId(systemDark), systemDark);
        QCOMPARE(reappliedTheme.id, reappliedId);
        QCOMPARE(python->color(QsciLexerPython::FunctionMethodName),
                 reappliedTheme.syntaxColors.value(NotePeek::SyntaxRole::Function));

        preferences.setDarkSyntaxThemeId(previousDarkTheme);
        preferences.setLightSyntaxThemeId(previousLightTheme);
    }

    void exposesAllSyntaxThemesInPreferences()
    {
        PreferencesDialog dialog;
        QComboBox *darkCombo =
            dialog.findChild<QComboBox *>(QStringLiteral("darkSyntaxThemeCombo"));
        QComboBox *lightCombo =
            dialog.findChild<QComboBox *>(QStringLiteral("lightSyntaxThemeCombo"));
        QWidget *darkRow = dialog.findChild<QWidget *>(QStringLiteral("darkSyntaxThemeRow"));
        QWidget *lightRow = dialog.findChild<QWidget *>(QStringLiteral("lightSyntaxThemeRow"));
        QVERIFY(darkCombo);
        QVERIFY(lightCombo);
        QVERIFY(darkRow);
        QVERIFY(lightRow);
        // The single fixed-theme selector and the "follow system" checkbox
        // were removed: syntax highlighting always tracks the system theme,
        // so both rows are always visible.
        QVERIFY(!dialog.findChild<QComboBox *>(QStringLiteral("syntaxThemeCombo")));
        QVERIFY(!dialog.findChild<QCheckBox *>(
            QStringLiteral("syntaxThemeFollowsSystemCheckBox")));
        QVERIFY(!dialog.findChild<QWidget *>(QStringLiteral("syntaxThemeRow")));
        QVERIFY(!darkRow->isHidden());
        QVERIFY(!lightRow->isHidden());
        QVERIFY(darkCombo->count() >= 5);
        QVERIFY(lightCombo->count() >= 5);
        QCOMPARE(darkCombo->findData(QStringLiteral("upstream-vscode-dark-2026")) >= 0, true);
        QCOMPARE(lightCombo->findData(QStringLiteral("upstream-vscode-light-2026")) >= 0, true);
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("importSyntaxThemeButton")));
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("openSyntaxThemeFolderButton")));
    }

    void preferencesSidebarSearchAndThemeRefreshStayLive()
    {
        PreferencesDialog dialog;
        dialog.show();
        QCoreApplication::processEvents();
        auto *search =
            dialog.findChild<QLineEdit *>(QStringLiteral("settingsSearch"));
        auto *sections =
            dialog.findChild<QListWidget *>(QStringLiteral("sectionList"));
        QVERIFY(search);
        QVERIFY(sections);
        QCOMPARE(sections->count(), 4);
        QVERIFY(dialog.width() <= 700);
        QVERIFY(dialog.height() <= 320);
        QCOMPARE(search->width(), sections->viewport()->width());

        QLabel *sectionTitle = nullptr;
        QLabel *textFontLabel = nullptr;
        QLabel *codeFontLabel = nullptr;
        const QList<QLabel *> labels = dialog.findChildren<QLabel *>();
        for (QLabel *label : labels)
        {
            if (label->objectName() == QLatin1String("sectionTitle")
                && label->text() == QLatin1String("Editor"))
                sectionTitle = label;
            else if (label->text() == QLatin1String("Text font"))
                textFontLabel = label;
            else if (label->text() == QLatin1String("Code font"))
                codeFontLabel = label;
        }
        QVERIFY(sectionTitle);
        QVERIFY(textFontLabel);
        QVERIFY(codeFontLabel);
        QVERIFY(sectionTitle->font().bold());
        // Bold CJK glyphs carry about 4 px of visible left side bearing. The
        // title widget is deliberately offset so its painted edge, rather
        // than merely its widget geometry, aligns with the regular labels.
        QCOMPARE(sectionTitle->mapTo(&dialog, QPoint()).x() + 4,
                 textFontLabel->mapTo(&dialog, QPoint()).x());
        QVERIFY(codeFontLabel->mapTo(&dialog, QPoint()).y()
                - textFontLabel->mapTo(&dialog, QPoint()).y() <= 36);

        auto *darkCombo = dialog.findChild<QComboBox *>(
            QStringLiteral("darkSyntaxThemeCombo"));
        QVERIFY(darkCombo);
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("applyButton")));
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("cancelButton")));
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("okButton")));

        AppPreferences &preferences = AppPreferences::instance();
        auto *statusBarCheckBox = dialog.findChild<QCheckBox *>(
            QStringLiteral("showStatusBarCheckBox"));
        auto *gitIntegrationCheckBox = dialog.findChild<QCheckBox *>(
            QStringLiteral("gitIntegrationEnabledCheckBox"));
        QVERIFY(statusBarCheckBox);
        QVERIFY(gitIntegrationCheckBox);
        QSignalSpy appliedSpy(&dialog, &PreferencesDialog::applied);

        const bool previousGitIntegration = preferences.gitIntegrationEnabled();
        gitIntegrationCheckBox->setChecked(!previousGitIntegration);
        QCOMPARE(preferences.gitIntegrationEnabled(), !previousGitIntegration);
        QCOMPARE(appliedSpy.count(), 1);
        gitIntegrationCheckBox->setChecked(previousGitIntegration);
        QCOMPARE(preferences.gitIntegrationEnabled(), previousGitIntegration);
        QCOMPARE(appliedSpy.count(), 2);

        const bool previousStatusBar = preferences.showStatusBar();
        statusBarCheckBox->setChecked(!previousStatusBar);
        QCOMPARE(preferences.showStatusBar(), !previousStatusBar);
        QCOMPARE(appliedSpy.count(), 3);
        statusBarCheckBox->setChecked(previousStatusBar);
        QCOMPARE(preferences.showStatusBar(), previousStatusBar);
        QCOMPARE(appliedSpy.count(), 4);

        const QString previousDarkTheme = preferences.darkSyntaxThemeId();
        const int alternativeTheme = (darkCombo->currentIndex() + 1) % darkCombo->count();
        darkCombo->setCurrentIndex(alternativeTheme);
        QCOMPARE(preferences.darkSyntaxThemeId(), darkCombo->currentData().toString());
        QCOMPARE(appliedSpy.count(), 5);
        darkCombo->setCurrentIndex(darkCombo->findData(previousDarkTheme));
        QCOMPARE(preferences.darkSyntaxThemeId(), previousDarkTheme);
        QCOMPARE(appliedSpy.count(), 6);

        search->setText(QStringLiteral("Markdown"));
        QVERIFY(sections->item(0)->isHidden());
        QVERIFY(!sections->item(1)->isHidden());
        QVERIFY(sections->item(2)->isHidden());
        QVERIFY(sections->item(3)->isHidden());
        QCOMPARE(sections->currentItem(), sections->item(1));

        search->setText(QStringLiteral("Brackets"));
        QVERIFY(sections->item(0)->isHidden());
        QVERIFY(sections->item(1)->isHidden());
        QVERIFY(!sections->item(2)->isHidden());
        QVERIFY(sections->item(3)->isHidden());
        QCOMPARE(sections->currentItem(), sections->item(2));

        search->setText(QStringLiteral("Git"));
        QVERIFY(sections->item(0)->isHidden());
        QVERIFY(sections->item(1)->isHidden());
        QVERIFY(sections->item(2)->isHidden());
        QVERIFY(!sections->item(3)->isHidden());
        QCOMPARE(sections->currentItem(), sections->item(3));

        search->setText(QStringLiteral("No such setting"));
        QVERIFY(dialog.findChild<QStackedWidget *>()->isHidden());

        search->clear();
        for (int row = 0; row < sections->count(); ++row)
            QVERIFY(!sections->item(row)->isHidden());
        QVERIFY(!dialog.findChild<QStackedWidget *>()->isHidden());

        // Style and palette changes can arrive while the modal dialog is
        // open. Clearing the sheet emits StyleChange; the queued refresh must
        // rebuild the appearance without requiring the dialog to be reopened.
        dialog.setStyleSheet(QString());
        QTRY_VERIFY(!dialog.styleSheet().isEmpty());

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QStyleHints *styleHints = QGuiApplication::styleHints();
        const Qt::ColorScheme originalScheme = styleHints->colorScheme();
        const Qt::ColorScheme requestedScheme =
            originalScheme == Qt::ColorScheme::Dark
            ? Qt::ColorScheme::Light
            : Qt::ColorScheme::Dark;
        const QColor originalBackground = dialog.palette().color(QPalette::Window);

        styleHints->setColorScheme(requestedScheme);
        QCoreApplication::processEvents();
        if (styleHints->colorScheme() == requestedScheme)
        {
            QTRY_COMPARE(dialog.palette().color(QPalette::Window),
                         ThemeColors::Preferences::windowBg());
            QVERIFY(dialog.palette().color(QPalette::Window) != originalBackground);
        }

        styleHints->setColorScheme(originalScheme);
        QCoreApplication::processEvents();
#endif
    }

    void statusBarVisibilityFollowsPreferences()
    {
        AppPreferences &preferences = AppPreferences::instance();
        const bool previousVisibility = preferences.showStatusBar();

        preferences.setShowStatusBar(false);
        MainWindow hiddenWindow;
        auto *hiddenStatusBar = hiddenWindow.findChild<QsciEditorInfoStatusBar *>();
        QVERIFY(hiddenStatusBar);
        QVERIFY(hiddenStatusBar->isHidden());

        PreferencesDialog dialog;
        auto *statusBarCheckBox =
            dialog.findChild<QCheckBox *>(QStringLiteral("showStatusBarCheckBox"));
        QVERIFY(statusBarCheckBox);
        QVERIFY(!statusBarCheckBox->isChecked());

        preferences.setShowStatusBar(true);
        MainWindow visibleWindow;
        visibleWindow.show();
        auto *visibleStatusBar = visibleWindow.findChild<QsciEditorInfoStatusBar *>();
        QVERIFY(visibleStatusBar);
        QTRY_VERIFY(visibleStatusBar->isVisible());

        preferences.setShowStatusBar(previousVisibility);
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

    void asynchronousFormattingNeverOverwritesANewerEdit()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString jsonPath = dir.filePath(QStringLiteral("concurrent.json"));
        writeFile(jsonPath, QByteArrayLiteral("{\"value\":1}\n"));

        MainWindow window(nullptr, {jsonPath});
        window.show();
        auto *action = window.findChild<QAction *>(QStringLiteral("action_formatCode"));
        QVERIFY(action);

        CustomEditor *editor = nullptr;
        QTRY_VERIFY([&]() {
            for (CustomEditor *candidate : window.findChildren<CustomEditor *>())
            {
                if (QFileInfo(candidate->filePath()) == QFileInfo(jsonPath))
                {
                    editor = candidate;
                    return candidate->text() == QStringLiteral("{\"value\":1}\n");
                }
            }
            return false;
        }());
        QTRY_VERIFY(action->isEnabled());

        action->trigger();
        QVERIFY(!action->isEnabled());
        editor->setCursorPosition(0, 1);
        editor->insert(QStringLiteral("user-edit"));
        const QString editedText = editor->text();

        QTRY_VERIFY_WITH_TIMEOUT(action->isEnabled(), 3000);
        QCOMPARE(editor->text(), editedText);
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

    void fileTreeExposesGitStatusForFilesAndDirectories()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("notes")));
        const QString modifiedPath = dir.filePath(QStringLiteral("notes/modified.md"));
        const QString untrackedPath = dir.filePath(QStringLiteral("notes/new.md"));
        const QString copiedPath = dir.filePath(QStringLiteral("notes/copied.md"));
        const QString conflictPath = dir.filePath(QStringLiteral("notes/conflict.md"));
        writeFile(modifiedPath, QByteArrayLiteral("changed\n"));
        writeFile(untrackedPath, QByteArrayLiteral("new\n"));
        writeFile(copiedPath, QByteArrayLiteral("copied\n"));
        writeFile(conflictPath, QByteArrayLiteral("conflict\n"));

        QFileSystemModel model;
        model.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
        FileTypeFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        model.setRootPath(dir.path());
        proxy.setVisibleRootDirectory(dir.path());

        GitRepositorySnapshot snapshot;
        snapshot.repositoryRoot = dir.path();
        GitFileStatus modified;
        modified.path = QStringLiteral("notes/modified.md");
        modified.workTreeStatus = QLatin1Char('M');
        GitFileStatus untracked;
        untracked.recordType = GitChangeRecordType::Untracked;
        untracked.path = QStringLiteral("notes/new.md");
        GitFileStatus copied;
        copied.recordType = GitChangeRecordType::RenamedOrCopied;
        copied.renameOrCopyScore = QStringLiteral("C100");
        copied.path = QStringLiteral("notes/copied.md");
        GitFileStatus conflict;
        conflict.recordType = GitChangeRecordType::Unmerged;
        conflict.path = QStringLiteral("notes/conflict.md");
        snapshot.files = {modified, untracked, copied, conflict};
        proxy.setGitRepositorySnapshot(snapshot);

        QModelIndex modifiedIndex;
        QModelIndex directoryIndex;
        QModelIndex copiedIndex;
        QModelIndex conflictIndex;
        QTRY_VERIFY([&]() {
            modifiedIndex = proxy.mapFromSource(model.index(modifiedPath));
            directoryIndex = proxy.mapFromSource(
                model.index(dir.filePath(QStringLiteral("notes"))));
            copiedIndex = proxy.mapFromSource(model.index(copiedPath));
            conflictIndex = proxy.mapFromSource(model.index(conflictPath));
            return modifiedIndex.isValid() && directoryIndex.isValid()
                   && copiedIndex.isValid() && conflictIndex.isValid();
        }());
        QCOMPARE(proxy.data(modifiedIndex,
                            FileTypeFilterProxyModel::GitStatusTextRole).toString(),
                 QStringLiteral("M"));
        QCOMPARE(proxy.data(directoryIndex,
                            FileTypeFilterProxyModel::GitStatusTextRole).toString(),
                 QStringLiteral("●"));
        QVERIFY(proxy.data(directoryIndex, Qt::ToolTipRole).toString()
                    .contains(QStringLiteral("Git changes: 4")));
        QVERIFY(proxy.data(modifiedIndex, Qt::ToolTipRole).toString()
                    .contains(QStringLiteral("Git status: M")));
        QCOMPARE(proxy.data(copiedIndex,
                            FileTypeFilterProxyModel::GitStatusTextRole).toString(),
                 QStringLiteral("C"));
        QCOMPARE(proxy.data(conflictIndex,
                            FileTypeFilterProxyModel::GitStatusTextRole).toString(),
                 QStringLiteral("!"));
    }

    void gitIntegrationPreferenceAppliesLive()
    {
        const QString git = GitExecutable::find();
        if (git.isEmpty())
            QSKIP("System Git is unavailable", 0);

        GitIntegrationPreferenceGuard gitPreference(false);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("tracked.md"));
        writeFile(filePath, QByteArrayLiteral("original\n"));
        QVERIFY(runGit(git, dir.path(),
                       {QStringLiteral("init"), QStringLiteral("--quiet")}));
        QVERIFY(runGit(git, dir.path(),
                       {QStringLiteral("add"), QStringLiteral("tracked.md")}));
        QVERIFY(runGit(git, dir.path(),
                       {QStringLiteral("-c"), QStringLiteral("user.name=NotePeek Test"),
                        QStringLiteral("-c"), QStringLiteral("user.email=notepeek@example.invalid"),
                        QStringLiteral("-c"), QStringLiteral("commit.gpgSign=false"),
                        QStringLiteral("commit"), QStringLiteral("--quiet"),
                        QStringLiteral("-m"), QStringLiteral("initial")}));

        MainWindow window(nullptr, {filePath});
        window.show();
        QVERIFY(!window.findChild<GitRepositoryService *>());
        auto *gitLabel = window.findChild<QLabel *>(
            QStringLiteral("statusGitRepository"));
        QVERIFY(gitLabel);
        QVERIFY(!gitLabel->isVisible());

        QAction *preferencesAction = window.findChild<QAction *>(
            QStringLiteral("action_preferences"));
        QVERIFY(preferencesAction);
        auto setThroughPreferences = [&](bool enabled) {
            bool dialogFound = false;
            QTimer::singleShot(0, &window, [&]() {
                auto *dialog = qobject_cast<PreferencesDialog *>(
                    QApplication::activeModalWidget());
                if (!dialog)
                {
                    if (QWidget *modal = QApplication::activeModalWidget())
                        modal->close();
                    return;
                }
                dialogFound = true;
                auto *checkBox = dialog->findChild<QCheckBox *>(
                    QStringLiteral("gitIntegrationEnabledCheckBox"));
                if (checkBox)
                    checkBox->setChecked(enabled);
                else
                    dialogFound = false;
                dialog->accept();
            });
            preferencesAction->trigger();
            return dialogFound;
        };

        QVERIFY(setThroughPreferences(true));
        QCOMPARE(AppPreferences::instance().gitIntegrationEnabled(), true);
        auto *service = window.findChild<GitRepositoryService *>();
        QVERIFY(service);
        QTRY_COMPARE_WITH_TIMEOUT(service->state(), GitRepositoryState::Ready, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(gitLabel->isVisible(), 5000);

        QVERIFY(setThroughPreferences(false));
        QCOMPARE(AppPreferences::instance().gitIntegrationEnabled(), false);
        QVERIFY(!window.findChild<GitRepositoryService *>());
        QVERIFY(!gitLabel->isVisible());
    }

    void nonRepositoryRediscoveryWaitsForGitMetadata()
    {
        const QString git = GitExecutable::find();
        if (git.isEmpty())
            QSKIP("System Git is unavailable", 0);

        GitIntegrationPreferenceGuard gitPreference(true);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("note.md"));
        writeFile(filePath, QByteArrayLiteral("note\n"));

        MainWindow window(nullptr, {filePath});
        window.show();
        auto *service = window.findChild<GitRepositoryService *>();
        QVERIFY(service);
        QTRY_COMPARE_WITH_TIMEOUT(service->state(), GitRepositoryState::NotRepository, 5000);

        QSignalSpy states(service, &GitRepositoryService::stateChanged);
        QEvent activateWithoutRepository(QEvent::WindowActivate);
        QCoreApplication::sendEvent(&window, &activateWithoutRepository);
        QTest::qWait(500);
        QCOMPARE(states.count(), 0);

        QVERIFY(runGit(git, dir.path(),
                       {QStringLiteral("init"), QStringLiteral("--quiet")}));
        QEvent activateWithRepository(QEvent::WindowActivate);
        QCoreApplication::sendEvent(&window, &activateWithRepository);
        QTRY_COMPARE_WITH_TIMEOUT(service->state(), GitRepositoryState::Ready, 5000);
    }

    void openingGitProjectShowsBranchAndFileStatus()
    {
        const QString git = GitExecutable::find();
        if (git.isEmpty())
            QSKIP("System Git is unavailable", 0);

        GitIntegrationPreferenceGuard gitPreference(true);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("tracked.md"));
        writeFile(filePath, QByteArrayLiteral("original\n"));
        QVERIFY(runGit(git, dir.path(),
                       {QStringLiteral("init"), QStringLiteral("--quiet")}));
        QVERIFY(runGit(git, dir.path(),
                       {QStringLiteral("add"), QStringLiteral("tracked.md")}));
        QVERIFY(runGit(git, dir.path(),
                       {QStringLiteral("-c"), QStringLiteral("user.name=NotePeek Test"),
                        QStringLiteral("-c"), QStringLiteral("user.email=notepeek@example.invalid"),
                        QStringLiteral("-c"), QStringLiteral("commit.gpgSign=false"),
                        QStringLiteral("commit"), QStringLiteral("--quiet"),
                        QStringLiteral("-m"), QStringLiteral("initial")}));
        writeFile(filePath, QByteArrayLiteral("modified\n"));

        MainWindow window(nullptr, {filePath});
        window.show();
        QLabel *gitLabel = window.findChild<QLabel *>(
            QStringLiteral("statusGitRepository"));
        QVERIFY(gitLabel);
        QTRY_VERIFY_WITH_TIMEOUT(gitLabel->isVisible(), 10000);
        QVERIFY(!gitLabel->text().startsWith(QStringLiteral("Git:")));
        QVERIFY(gitLabel->text().contains(QStringLiteral("•1")));

        FileTypeFilterProxyModel *proxy = nullptr;
        for (QTreeView *tree : window.findChildren<QTreeView *>())
        {
            proxy = qobject_cast<FileTypeFilterProxyModel *>(tree->model());
            if (proxy)
                break;
        }
        QVERIFY(proxy);
        auto *model = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
        QVERIFY(model);
        QModelIndex fileIndex;
        QTRY_VERIFY_WITH_TIMEOUT([&]() {
            const QString treeFilePath = QDir(model->rootPath()).filePath(
                QStringLiteral("tracked.md"));
            fileIndex = proxy->mapFromSource(model->index(treeFilePath));
            return fileIndex.isValid()
                   && proxy->data(fileIndex,
                                  FileTypeFilterProxyModel::GitStatusTextRole).toString()
                          == QStringLiteral("M");
        }(), 10000);

        const QString untrackedPath = dir.filePath(QStringLiteral("new.md"));
        writeFile(untrackedPath, QByteArrayLiteral("new\n"));
        QTRY_VERIFY_WITH_TIMEOUT(gitLabel->text().contains(QStringLiteral("•2")), 10000);
        QTRY_VERIFY_WITH_TIMEOUT([&]() {
            const QString treeFilePath = QDir(model->rootPath()).filePath(
                QStringLiteral("new.md"));
            const QModelIndex newFileIndex = proxy->mapFromSource(model->index(treeFilePath));
            return newFileIndex.isValid()
                   && proxy->data(newFileIndex,
                                  FileTypeFilterProxyModel::GitStatusTextRole).toString()
                          == QStringLiteral("U");
        }(), 10000);
    }

    void fileTreeClassifiesBinaryFilesOffTheUiThread()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString textPath = dir.filePath(QStringLiteral("visible.txt"));
        const QString binaryPath = dir.filePath(QStringLiteral("binary.txt"));
        writeFile(textPath, QByteArrayLiteral("plain text\n"));
        writeFile(binaryPath, QByteArray("prefix\0binary", 13));

        QFileSystemModel model;
        model.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
        FileTypeFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        model.setRootPath(dir.path());
        proxy.setVisibleRootDirectory(dir.path());

        QTRY_VERIFY([&]() {
            const QModelIndex source = model.index(textPath);
            return source.isValid() && proxy.mapFromSource(source).isValid();
        }());
        // Eligible extensions are provisionally visible while the bounded
        // background probe runs, then binary content is removed.
        proxy.mapFromSource(model.index(binaryPath));
        QTRY_VERIFY_WITH_TIMEOUT([&]() {
            const QModelIndex source = model.index(binaryPath);
            return source.isValid() && !proxy.mapFromSource(source).isValid();
        }(), 3000);
        QVERIFY(proxy.mapFromSource(model.index(textPath)).isValid());
    }

    void highlightsAllMatchesInEditor()
    {
        CustomEditor editor;
        editor.setText(QStringLiteral("foo bar foo\nbaz foo\n"));
        editor.setSelection(0, 0, 0, 3);

        FindReplaceManager manager;
        manager.setCurrentEditor(&editor);
        manager.setFindText(QStringLiteral("foo"));
        manager.highlightAllMatches();
        QCOMPARE(manager.searchResults().size(), 3);
        int fromLine = -1;
        int fromIndex = -1;
        int toLine = -1;
        int toIndex = -1;
        editor.getSelection(&fromLine, &fromIndex, &toLine, &toIndex);
        QCOMPARE(QVector<int>({fromLine, fromIndex, toLine, toIndex}),
                 QVector<int>({0, 0, 0, 3}));

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
