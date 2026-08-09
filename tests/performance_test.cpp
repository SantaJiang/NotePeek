#include "editor/customeditor.h"
#include "apppreferences.h"
#include "editor/documentformat.h"
#include "editor/documentpersistenceservice.h"
#include "editor/documentsavecoordinator.h"
#include "editor/documentcontroller.h"
#include "editor/editormanager.h"
#include "editor/editorpane.h"
#include "find/findreplacemanager.h"
#include "decorators/highlightedscrollbar.h"
#include "decorators/highlightedscrollbardecorator.h"
#include "tool/markdown/markdownpreviewer.h"
#include <NotePeek/semantictokens.h>
#include <NotePeek/syntaxtheme.h>
#include <NotePeek/syntaxstyler.h>
#include <Qsci/qscilexercpp.h>

#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

namespace {

qint64 measureSingleCharacterEdits(QsciScintilla &editor, int editCount)
{
    editor.setCursorPosition(0, 0);
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < editCount; ++i)
    {
        editor.insert(QStringLiteral("x"));
        editor.SendScintilla(QsciScintillaBase::SCI_DELETEBACK);
    }
    return timer.nsecsElapsed();
}

qint64 measureScrolling(QsciScintilla &editor, int maximumLine, int steps)
{
    editor.resize(900, 700);
    editor.show();
    QApplication::processEvents();
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < steps; ++i)
    {
        const int line = (i * maximumLine) / qMax(steps - 1, 1);
        editor.SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, line);
        editor.viewport()->repaint();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    return timer.nsecsElapsed();
}

qint64 medianScrollTime(QsciScintilla &editor, int maximumLine, int steps)
{
    QVector<qint64> samples;
    for (int i = 0; i < 5; ++i)
        samples.append(measureScrolling(editor, maximumLine, steps));
    std::sort(samples.begin(), samples.end());
    return samples.at(samples.size() / 2);
}

struct PairedScrollResult
{
    qint64 nativeMedianNs = 0;
    qint64 customizedMedianNs = 0;
    double medianRatio = 0.0;
};

PairedScrollResult measurePairedScrolling(QsciScintilla &nativeEditor,
                                          QsciScintilla &customizedEditor,
                                          int maximumLine, int steps)
{
    // Warm both renderers, then alternate order to cancel font-cache,
    // compositor and thermal drift. Separate five-run blocks can otherwise
    // report a false regression simply because the second block ran later.
    measureScrolling(nativeEditor, maximumLine, qMin(20, steps));
    measureScrolling(customizedEditor, maximumLine, qMin(20, steps));

    QVector<qint64> nativeSamples;
    QVector<qint64> customizedSamples;
    QVector<double> pairedRatios;
    for (int sample = 0; sample < 7; ++sample)
    {
        qint64 nativeNs = 0;
        qint64 customizedNs = 0;
        if ((sample & 1) == 0)
        {
            nativeNs = measureScrolling(nativeEditor, maximumLine, steps);
            customizedNs = measureScrolling(customizedEditor, maximumLine, steps);
        }
        else
        {
            customizedNs = measureScrolling(customizedEditor, maximumLine, steps);
            nativeNs = measureScrolling(nativeEditor, maximumLine, steps);
        }
        nativeSamples.append(nativeNs);
        customizedSamples.append(customizedNs);
        pairedRatios.append(nativeNs > 0 ? double(customizedNs) / nativeNs : 0.0);
    }
    std::sort(nativeSamples.begin(), nativeSamples.end());
    std::sort(customizedSamples.begin(), customizedSamples.end());
    std::sort(pairedRatios.begin(), pairedRatios.end());
    return {nativeSamples.at(nativeSamples.size() / 2),
            customizedSamples.at(customizedSamples.size() / 2),
            pairedRatios.at(pairedRatios.size() / 2)};
}

QString makeCppFixture(int targetSize)
{
    QString text;
    text.reserve(targetSize + 256);
    while (text.size() < targetSize)
        text.append(QStringLiteral("Widget createWidget(int count) { return object.title + makeTitle(count); }\n"));
    return text;
}

} // namespace

class PerformanceTest : public QObject
{
    Q_OBJECT

private slots:
    void decodesTenMegabytesWithinSmokeBudget()
    {
        QByteArray input;
        input.reserve(10 * 1024 * 1024);
        while (input.size() < 10 * 1024 * 1024)
            input.append("0123456789 abcdefghijklmnopqrstuvwxyz\r\n");

        QElapsedTimer timer;
        timer.start();
        QString text;
        DocumentFormat format;
        QString error;
        QVERIFY2(DocumentFormatCodec::decode(input, &text, &format, &error), qPrintable(error));
        QVERIFY2(timer.elapsed() < 5000, "10 MB decoding exceeded the 5 second smoke budget");
        QCOMPARE(format.lineEnding, DocumentLineEnding::CrLf);
    }

    void snapshotsUtf8WithoutQStringRoundTrip()
    {
        CustomEditor editor;
        editor.setDocumentFormat({DocumentEncoding::Utf8, DocumentLineEnding::Lf});
        editor.setText(QString(2 * 1024 * 1024, QLatin1Char('x')));
        DocumentSaveCoordinator coordinator;
        DocumentPersistenceService persistence(&coordinator);

        QByteArray snapshot;
        QString error;
        QElapsedTimer timer;
        timer.start();
        QVERIFY2(persistence.saveEditorSnapshot(&editor, &snapshot, &error), qPrintable(error));
        QVERIFY2(timer.elapsed() < 2000, "2 MB snapshot exceeded the 2 second smoke budget");
        QCOMPARE(snapshot.size(), 2 * 1024 * 1024);
    }

    void asynchronouslyOpensTenMegabytesWithinSmokeBudget()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("large.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray line("0123456789 abcdefghijklmnopqrstuvwxyz\n");
        qint64 written = 0;
        while (written < 10 * 1024 * 1024)
        {
            const qint64 remaining = 10 * 1024 * 1024 - written;
            const qint64 chunkSize = file.write(line.constData(), qMin<qint64>(line.size(), remaining));
            QVERIFY2(chunkSize > 0, "Failed to create the 10 MB performance fixture");
            written += chunkSize;
        }
        file.close();

        EditorPane pane;
        DocumentController controller(&pane, false);
        QElapsedTimer timer;
        timer.start();
        controller.openFile(path);
        bool budgetIsValid = false;
        const int configuredBudget = qEnvironmentVariableIntValue("NOTEPEEK_OPEN_SMOKE_BUDGET_MS",
                                                                   &budgetIsValid);
        const int openBudgetMs = budgetIsValid && configuredBudget > 0 ? configuredBudget : 12000;
        QTRY_VERIFY_WITH_TIMEOUT(controller.currentEditor() && !controller.currentEditor()->isReadOnly(),
                                 openBudgetMs + 3000);
        QVERIFY2(timer.elapsed() < openBudgetMs,
                 "10 MB asynchronous open exceeded the configured smoke budget");
        QCOMPARE(controller.currentEditor()->length(), 10 * 1024 * 1024);
    }

    void highlightsAllMatchesWithinSmokeBudget()
    {
        CustomEditor editor;
        QString text;
        text.reserve(2 * 1024 * 1024);
        while (text.size() < 2 * 1024 * 1024)
            text.append(QStringLiteral("needle in a haystack of words and lines\n"));
        editor.setText(text);

        FindReplaceManager manager;
        manager.setCurrentEditor(&editor);
        manager.setFindText(QStringLiteral("needle"));

        QElapsedTimer timer;
        timer.start();
        manager.highlightAllMatches();
        bool highlightBudgetIsValid = false;
        const int configuredHighlightBudget = qEnvironmentVariableIntValue(
            "NOTEPEEK_HIGHLIGHT_SMOKE_BUDGET_MS", &highlightBudgetIsValid);
        const int highlightBudgetMs =
            highlightBudgetIsValid && configuredHighlightBudget > 0 ? configuredHighlightBudget : 5000;
        QVERIFY2(timer.elapsed() < highlightBudgetMs,
                 "2 MB highlight pass exceeded the configured smoke budget");
        QVERIFY(!manager.searchResults().isEmpty());
    }

    void rendersMarkdownPreviewWithinSmokeBudget()
    {
        QString markdown;
        markdown.reserve(1024 * 1024);
        while (markdown.size() < 1024 * 1024)
            markdown.append(QStringLiteral("# Heading\n\nSome *emphasis*, a [link](https://example.com) "
                                           "and `code`.\n\n- item one\n- item two\n\n"));

        qRegisterMetaType<MarkdownRenderResult>("MarkdownRenderResult");
        MarkdownPreviewer previewer;
        QSignalSpy rendered(&previewer, &MarkdownPreviewer::previewRendered);
        QElapsedTimer timer;
        timer.start();
        const quint64 requestId = previewer.renderPreviewAsync(markdown,
                                                               QStringLiteral("/tmp/perf.md"),
                                                               nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(rendered.count() >= 1, 15000);
        QVERIFY2(timer.elapsed() < 10000, "1 MB Markdown render exceeded the 10 second smoke budget");
        const auto result = rendered.takeFirst().first().value<MarkdownRenderResult>();
        QCOMPARE(result.requestId, requestId);
        QVERIFY(!result.htmlDocument.isEmpty());
    }

    void reportsSemanticHighlightPerformanceAudit()
    {
        const QString fixture = makeCppFixture(1024 * 1024);
        constexpr int editCount = 300;

        QsciLexerCPP nativeLexer;
        QsciScintilla nativeEditor;
        nativeEditor.setLexer(&nativeLexer);
        nativeEditor.setText(fixture);
        const qint64 nativeEditNs = measureSingleCharacterEdits(nativeEditor, editCount);

        QsciLexerCPP customizedLexer;
        CustomEditor customizedEditor;
        customizedEditor.setLexer(&customizedLexer);
        customizedEditor.setText(fixture);
        const qint64 customizedEditNs = measureSingleCharacterEdits(customizedEditor, editCount);

        QElapsedTimer snapshotTimer;
        snapshotTimer.start();
        NotePeek::SemanticDocument document;
        document.languageId = QStringLiteral("C++");
        document.text = customizedEditor.text();
        const qint64 snapshotNs = snapshotTimer.nsecsElapsed();

        NotePeek::BasicSemanticTokenProvider provider;
        QElapsedTimer providerTimer;
        providerTimer.start();
        const QVector<NotePeek::SemanticToken> tokens = provider.tokensForDocument(document);
        const qint64 providerNs = providerTimer.nsecsElapsed();

        const int maximumLine = qMax(0, nativeEditor.lines() - 40);
        constexpr int scrollSteps = 180;
        const qint64 nativeScrollNs = measureScrolling(nativeEditor, maximumLine, scrollSteps);
        const qint64 customizedScrollNs = measureScrolling(customizedEditor, maximumLine, scrollSteps);

        NotePeek::SyntaxPalette semanticColors;
        semanticColors.insert(NotePeek::SyntaxRole::Type, QColor(QStringLiteral("#4EC9B0")));
        semanticColors.insert(NotePeek::SyntaxRole::Function, QColor(QStringLiteral("#DCDCAA")));
        semanticColors.insert(NotePeek::SyntaxRole::Variable, QColor(QStringLiteral("#9CDCFE")));
        semanticColors.insert(NotePeek::SyntaxRole::Property, QColor(QStringLiteral("#9CDCFE")));
        semanticColors.insert(NotePeek::SyntaxRole::Builtin, QColor(QStringLiteral("#D7BA7D")));
        customizedEditor.setSemanticTokenColors(semanticColors);
        QElapsedTimer applyTimer;
        applyTimer.start();
        QVERIFY(customizedEditor.setSemanticTokens(customizedEditor.documentRevision(), tokens));
        const qint64 applyNs = applyTimer.nsecsElapsed();
        const qint64 semanticScrollNs = measureScrolling(customizedEditor, maximumLine, scrollSteps);

        qInfo().nospace()
            << "semantic-audit bytes=" << fixture.toUtf8().size()
            << " edits=" << editCount
            << " nativeEditMs=" << nativeEditNs / 1000000.0
            << " customizedEditMs=" << customizedEditNs / 1000000.0
            << " ratio=" << (nativeEditNs > 0 ? double(customizedEditNs) / nativeEditNs : 0.0)
            << " uiSnapshotMs=" << snapshotNs / 1000000.0
            << " providerMs=" << providerNs / 1000000.0
            << " tokens=" << tokens.size()
            << " nativeScrollMs=" << nativeScrollNs / 1000000.0
            << " customizedScrollMs=" << customizedScrollNs / 1000000.0
            << " applyTokensMs=" << applyNs / 1000000.0
            << " semanticScrollMs=" << semanticScrollNs / 1000000.0
            << " semanticScrollRatio=" << (customizedScrollNs > 0
                                               ? double(semanticScrollNs) / customizedScrollNs : 0.0);
    }

    void viewportSemanticHighlightingMeetsInteractiveBudget()
    {
        const QString fixture = makeCppFixture(1024 * 1024);
        constexpr int scrollSteps = 180;

        QsciLexerCPP nativeLexer;
        QsciScintilla nativeEditor;
        nativeEditor.setMargins(2);
        nativeEditor.setMarginType(0, QsciScintilla::NumberMargin);
        nativeEditor.setFolding(QsciScintilla::NoFoldStyle, 1);
        nativeEditor.setScrollWidth(1);
        nativeEditor.setScrollWidthTracking(true);
        nativeEditor.setTabWidth(4);
        nativeEditor.setIndentationWidth(4);
        nativeEditor.setCaretLineVisible(true);
        nativeEditor.setWrapMode(QsciScintilla::WrapNone);
        nativeEditor.setIndentationGuides(true);
        const QFont baselineFont = AppPreferences::instance().codeFont();
        for (int style = 0; style <= QsciScintillaBase::STYLE_MAX; ++style)
            nativeLexer.setFont(baselineFont, style);
        nativeEditor.setLexer(&nativeLexer);
        nativeEditor.setText(fixture);
        nativeEditor.setMarginsFont(baselineFont);
        nativeEditor.setMarginWidth(0, QStringLiteral("99999 "));
        nativeEditor.resize(900, 700);
        nativeEditor.show();
        const int nativeSearchIndicator = nativeEditor.indicatorDefine(
            QsciScintilla::RoundBoxIndicator);
        nativeEditor.SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT,
                                   nativeSearchIndicator);

        EditorManager manager;
        CustomEditor *incrementalEditor = manager.createEditor();
        incrementalEditor->resize(900, 700);
        incrementalEditor->show();
        incrementalEditor->setText(fixture);
        manager.updateEditorSyntax(incrementalEditor, QStringLiteral("viewport.cpp"));
        QTRY_VERIFY_WITH_TIMEOUT(incrementalEditor->semanticTokenCount() > 0, 2000);

        // Keep the pre-existing find-result scrollbar interaction in the hard
        // gate. Hundreds of markers are cached into one overlay layer and must
        // not turn a common search into a per-frame traversal.
        auto *scrollDecorator = incrementalEditor->findChild<HighlightedScrollBarDecorator *>();
        QVERIFY(scrollDecorator);
        incrementalEditor->SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT,
                                          scrollDecorator->getSearchIndicator());
        int expectedSearchMarkers = 0;
        for (int line = 0; line < incrementalEditor->lines(); line += 40)
        {
            const long position = incrementalEditor->SendScintilla(
                QsciScintillaBase::SCI_POSITIONFROMLINE, line);
            incrementalEditor->SendScintilla(QsciScintillaBase::SCI_INDICATORFILLRANGE,
                                              position, 6L);
            const long nativePosition = nativeEditor.SendScintilla(
                QsciScintillaBase::SCI_POSITIONFROMLINE, line);
            nativeEditor.SendScintilla(QsciScintillaBase::SCI_INDICATORFILLRANGE,
                                       nativePosition, 6L);
            ++expectedSearchMarkers;
        }
        scrollDecorator->notify();
        QApplication::processEvents();
        QCOMPARE(scrollDecorator->getScrollBar()->searchMarkerCount(), expectedSearchMarkers);

        const int maximumLine = qMax(0, nativeEditor.lines() - 40);
        const PairedScrollResult scrollResult = measurePairedScrolling(
            nativeEditor, *incrementalEditor, maximumLine, scrollSteps);
        const qint64 nativeNs = scrollResult.nativeMedianNs;
        const qint64 incrementalNs = scrollResult.customizedMedianNs;
        const double ratio = scrollResult.medianRatio;

        // Let the single coalesced post-scroll viewport request complete.
        QTest::qWait(100);
        QTRY_VERIFY_WITH_TIMEOUT(!incrementalEditor->property("semanticAnalysisInFlight").toBool(), 2000);
        const qint64 workerNs = incrementalEditor->property("semanticWorkerNanoseconds").toLongLong();
        const qint64 applyNs = incrementalEditor->property("semanticApplyNanoseconds").toLongLong();
        const int snapshotBytes = incrementalEditor->property("semanticSnapshotBytes").toInt();

        qInfo().nospace()
            << "viewport-semantic sameFeatureNativeScrollMs=" << nativeNs / 1000000.0
            << " incrementalScrollMs=" << incrementalNs / 1000000.0
            << " ratio=" << ratio
            << " workerMs=" << workerNs / 1000000.0
            << " uiApplyMs=" << applyNs / 1000000.0
            << " snapshotBytes=" << snapshotBytes
            << " cachedTokens=" << incrementalEditor->semanticTokenCount();

#ifdef NDEBUG
        constexpr double maximumScrollRatio = 1.05;
        const char *scrollBudgetMessage =
            "Release viewport semantic highlighting regressed median scrolling by more than 5%";
#else
        // The distributable is always measured with the strict 5% gate above.
        // Debug builds leave Scintilla's decoration-run traversal unoptimized;
        // retain a 10% diagnostic guard without shrinking the visible-token
        // window or changing the shipped interaction to satisfy an O0 artifact.
        constexpr double maximumScrollRatio = 1.10;
        const char *scrollBudgetMessage =
            "Debug viewport semantic highlighting regressed median scrolling by more than 10%";
#endif
        QVERIFY2(ratio <= maximumScrollRatio, scrollBudgetMessage);
        // The worker is given a 2 ms design budget (kWorkerBudgetNs in
        // editormanager.cpp) but the provider checks it between work chunks, so
        // the measured elapsed time routinely overshoots by a fraction of a
        // millisecond and lands right on the 2 ms boundary — flaky on slower CI
        // runners and even under local background load.  Keep 2 ms as the
        // shipped target but let the smoke budget be widened per runner via the
        // same env-var pattern as NOTEPEEK_OPEN_SMOKE_BUDGET_MS, so a gross
        // regression still fails while chunk-granularity overshoot does not.
        bool workerBudgetIsValid = false;
        const int configuredWorkerBudget = qEnvironmentVariableIntValue(
            "NOTEPEEK_SEMANTIC_WORKER_BUDGET_MS", &workerBudgetIsValid);
        const int workerBudgetMs =
            workerBudgetIsValid && configuredWorkerBudget > 0 ? configuredWorkerBudget : 2;
        QVERIFY2(workerNs <= qint64(workerBudgetMs) * 1000 * 1000,
                 "Viewport semantic worker exceeded its configured budget");
        QVERIFY2(applyNs <= 2 * 1000 * 1000, "Applying viewport semantic tokens exceeded 2 ms on the UI thread");
        QVERIFY2(snapshotBytes < 128 * 1024, "Viewport snapshot exceeded the bounded memory budget");
        QVERIFY2(incrementalEditor->semanticTokenCount() < 2000, "Viewport token cache grew beyond its initial bound");
    }

    void profilesEditorRenderingComponents()
    {
        const QString fixture = makeCppFixture(1024 * 1024);
        constexpr int scrollSteps = 180;

        auto configureContent = [&fixture](QsciScintilla &editor, QsciLexerCPP &lexer) {
            editor.setLexer(&lexer);
            editor.setText(fixture);
            editor.resize(900, 700);
            editor.show();
            QApplication::processEvents();
        };

        QsciLexerCPP nativeLexer;
        QsciScintilla nativeEditor;
        configureContent(nativeEditor, nativeLexer);
        const int maximumLine = qMax(0, nativeEditor.lines() - 40);
        const qint64 nativeNs = medianScrollTime(nativeEditor, maximumLine, scrollSteps);
        nativeEditor.hide();
        QApplication::processEvents();

        QsciLexerCPP bareLexer;
        CustomEditor bareEditor;
        configureContent(bareEditor, bareLexer);
        const qint64 bareNs = medianScrollTime(bareEditor, maximumLine, scrollSteps);
        bareEditor.hide();
        QApplication::processEvents();

        QsciLexerCPP fontLexer;
        QsciScintilla fontEditor;
        const QFont codeFont = AppPreferences::instance().codeFont();
        for (int style = 0; style <= QsciScintillaBase::STYLE_MAX; ++style)
            fontLexer.setFont(codeFont, style);
        configureContent(fontEditor, fontLexer);
        const qint64 fontNs = medianScrollTime(fontEditor, maximumLine, scrollSteps);
        fontEditor.hide();
        QApplication::processEvents();

        QsciLexerCPP marginLexer;
        QsciScintilla marginEditor;
        marginEditor.setMargins(2);
        marginEditor.setMarginType(0, QsciScintilla::NumberMargin);
        configureContent(marginEditor, marginLexer);
        marginEditor.setMarginWidth(0, QStringLiteral("99999 "));
        const qint64 marginNs = medianScrollTime(marginEditor, maximumLine, scrollSteps);
        marginEditor.hide();
        QApplication::processEvents();

        QsciLexerCPP guideLexer;
        QsciScintilla guideEditor;
        guideEditor.setIndentationWidth(4);
        guideEditor.setIndentationGuides(true);
        configureContent(guideEditor, guideLexer);
        const qint64 guideNs = medianScrollTime(guideEditor, maximumLine, scrollSteps);
        guideEditor.hide();
        QApplication::processEvents();

        QsciLexerCPP configuredLexer;
        CustomEditor configuredEditor;
        configuredEditor.setMargins(2);
        configuredEditor.setMarginType(0, CustomEditor::NumberMargin);
        configuredEditor.setMarginWidth(0, 0);
        configuredEditor.setFolding(CustomEditor::NoFoldStyle, 1);
        configuredEditor.setScrollWidth(1);
        configuredEditor.setScrollWidthTracking(true);
        configuredEditor.setTabWidth(4);
        configuredEditor.setIndentationWidth(4);
        configuredEditor.setCaretLineVisible(true);
        configuredEditor.setBraceMatching(CustomEditor::NoBraceMatch);
        configuredEditor.setWrapMode(CustomEditor::WrapNone);
        configuredEditor.setUtf8(true);
        configureContent(configuredEditor, configuredLexer);
        const qint64 configuredNs = medianScrollTime(configuredEditor, maximumLine, scrollSteps);
        configuredEditor.hide();
        QApplication::processEvents();

        QsciLexerCPP decoratedLexer;
        CustomEditor decoratedEditor;
        configureContent(decoratedEditor, decoratedLexer);
        HighlightedScrollBarDecorator decorator(&decoratedEditor);
        QApplication::processEvents();
        const qint64 decoratedNs = medianScrollTime(decoratedEditor, maximumLine, scrollSteps);
        decoratedEditor.hide();
        QApplication::processEvents();

        QsciLexerCPP indicatorLexer;
        CustomEditor indicatorEditor;
        configureContent(indicatorEditor, indicatorLexer);
        indicatorEditor.indicatorDefine(QsciScintilla::RoundBoxIndicator);
        indicatorEditor.markerDefine(QsciScintilla::Circle);
        const qint64 indicatorNs = medianScrollTime(indicatorEditor, maximumLine, scrollSteps);
        indicatorEditor.hide();
        QApplication::processEvents();

        QsciLexerCPP featureLexer;
        QsciScintilla featureEditor;
        featureEditor.setMargins(2);
        featureEditor.setMarginType(0, QsciScintilla::NumberMargin);
        featureEditor.setFolding(QsciScintilla::NoFoldStyle, 1);
        featureEditor.setScrollWidth(1);
        featureEditor.setScrollWidthTracking(true);
        featureEditor.setTabWidth(4);
        featureEditor.setIndentationWidth(4);
        featureEditor.setCaretLineVisible(true);
        featureEditor.setWrapMode(QsciScintilla::WrapNone);
        featureEditor.setUtf8(true);
        featureEditor.setIndentationGuides(true);
        for (int style = 0; style <= QsciScintillaBase::STYLE_MAX; ++style)
            featureLexer.setFont(codeFont, style);
        const NotePeek::SyntaxTheme theme = NotePeek::SyntaxThemeCatalog::theme(
            QStringLiteral("notepeek-dark-modern"), true);
        NotePeek::SyntaxStyler::applyTheme(&featureLexer, theme.syntaxColors,
                                           theme.editorColor(QStringLiteral("background")),
                                           theme.editorColor(QStringLiteral("foreground")));
        configureContent(featureEditor, featureLexer);
        featureEditor.setMarginsFont(codeFont);
        featureEditor.setMarginWidth(0, QStringLiteral("99999 "));
        const qint64 featureNs = medianScrollTime(featureEditor, maximumLine, scrollSteps);
        featureEditor.hide();
        QApplication::processEvents();

        qInfo().nospace()
            << "render-components nativeMs=" << nativeNs / 1000000.0
            << " bareCustomMs=" << bareNs / 1000000.0
            << " bareRatio=" << double(bareNs) / nativeNs
            << " fontMs=" << fontNs / 1000000.0
            << " fontRatio=" << double(fontNs) / nativeNs
            << " marginMs=" << marginNs / 1000000.0
            << " marginRatio=" << double(marginNs) / nativeNs
            << " guidesMs=" << guideNs / 1000000.0
            << " guidesRatio=" << double(guideNs) / nativeNs
            << " configuredMs=" << configuredNs / 1000000.0
            << " configuredRatio=" << double(configuredNs) / nativeNs
            << " decoratedMs=" << decoratedNs / 1000000.0
            << " decoratedRatio=" << double(decoratedNs) / nativeNs
            << " indicatorsOnlyMs=" << indicatorNs / 1000000.0
            << " indicatorsOnlyRatio=" << double(indicatorNs) / nativeNs
            << " nativeFeaturesMs=" << featureNs / 1000000.0
            << " nativeFeaturesRatio=" << double(featureNs) / nativeNs;
    }
};

QTEST_MAIN(PerformanceTest)
#include "performance_test.moc"
