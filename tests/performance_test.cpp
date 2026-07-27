#include "editor/customeditor.h"
#include "editor/documentformat.h"
#include "editor/documentpersistenceservice.h"
#include "editor/documentsavecoordinator.h"
#include "editor/documentcontroller.h"
#include "editor/editorpane.h"
#include "find/findreplacemanager.h"
#include "tool/markdown/markdownpreviewer.h"

#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

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
        QVERIFY2(timer.elapsed() < 5000, "2 MB highlight pass exceeded the 5 second smoke budget");
        QVERIFY(!manager.searchResults().isEmpty());
    }

    void rendersMarkdownPreviewWithinSmokeBudget()
    {
        QString markdown;
        markdown.reserve(1024 * 1024);
        while (markdown.size() < 1024 * 1024)
            markdown.append(QStringLiteral("# Heading\n\nSome *emphasis*, a [link](https://example.com) "
                                           "and `code`.\n\n- item one\n- item two\n\n"));

        // Qt 5's QSignalSpy needs the custom result type registered to record it.
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
};

QTEST_MAIN(PerformanceTest)
#include "performance_test.moc"
