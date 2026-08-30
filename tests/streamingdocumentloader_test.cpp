#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/editorpane.h"
#include "editor/streamingdocumentloader.h"
#include "foundation/performanceconstants.h"

#include <QByteArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QByteArray makeLineBasedContent(qsizetype targetSize)
{
    QByteArray content;
    content.reserve(targetSize + 64);
    int line = 0;
    while (content.size() < targetSize)
    {
        content.append("line ");
        content.append(QByteArray::number(line++));
        content.append(" abcdefghijklmnopqrstuvwxyz\n");
    }
    content.truncate(targetSize);
    return content;
}

} // namespace

class StreamingDocumentLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<StreamDocumentMetadata>("StreamDocumentMetadata");
    }

    void loaderDeliversOrderedChunksAndMetadata()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("stream.txt"));
        const QByteArray content = makeLineBasedContent(5 * 1024 * 1024 + 137);
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(content), qint64(content.size()));
        }

        StreamingDocumentLoader loader;
        QSignalSpy finishedSpy(&loader, &StreamingDocumentLoader::finished);
        QList<QByteArray> chunks;
        QList<quint64> sequences;
        connect(&loader, &StreamingDocumentLoader::chunkReady,
                [&chunks, &sequences](quint64, quint64 sequence, const QByteArray &utf8) {
                    chunks.append(utf8);
                    sequences.append(sequence);
                });

        loader.start(42, path);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 30000);

        // Sequences are strictly increasing from 0.
        QVERIFY(!sequences.isEmpty());
        for (int i = 0; i < sequences.size(); ++i)
            QCOMPARE(sequences.at(i), quint64(i));

        QByteArray assembled;
        for (const QByteArray &chunk : chunks)
            assembled.append(chunk);
        QCOMPARE(assembled, content);

        const auto metadata = finishedSpy.takeFirst().at(1).value<StreamDocumentMetadata>();
        QCOMPARE(metadata.format.encoding, DocumentEncoding::Utf8);
        QCOMPARE(metadata.format.lineEnding, DocumentLineEnding::Lf);
        QVERIFY(metadata.inputBytes > 0);
        QVERIFY(metadata.outputUtf8Bytes == content.size());
    }

    void loaderBackpressureKeepsQueueBounded()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("large.txt"));
        const QByteArray content = makeLineBasedContent(32 * 1024 * 1024);
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(content), qint64(content.size()));
        }

        StreamingDocumentLoader loader;
        QSignalSpy finishedSpy(&loader, &StreamingDocumentLoader::finished);
        qint64 receivedBytes = 0;
        connect(&loader, &StreamingDocumentLoader::chunkReady,
                [&receivedBytes](quint64, quint64, const QByteArray &utf8) {
                    receivedBytes += utf8.size();
                });

        loader.start(1, path);
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 30000);
        QCOMPARE(receivedBytes, content.size());

        // The queue never holds more than the configured bound plus one chunk
        // worth of the largest decoded piece still being pushed.
        const qint64 peak = loader.peakQueuedBytes();
        QVERIFY2(peak <= StreamMaxQueuedBytes + StreamReadChunkBytes,
                 qPrintable(QStringLiteral("peak queued bytes %1 exceeded bound")
                                .arg(peak)));
    }

    void cancelStopsTheWorkerWithoutTerminalSignal()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("large.txt"));
        const QByteArray content = makeLineBasedContent(64 * 1024 * 1024);
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(content), qint64(content.size()));
        }

        auto *loader = new StreamingDocumentLoader;
        QSignalSpy finishedSpy(loader, &StreamingDocumentLoader::finished);
        QSignalSpy failedSpy(loader, &StreamingDocumentLoader::failed);

        loader->start(7, path);
        loader->cancelAll();
        loader->deleteLater();

        // Give any in-flight work a chance to run and confirm no terminal
        // signal leaked out after cancellation.
        QTest::qWait(200);
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(failedSpy.count(), 0);
    }

    void controllerOpensLargeFileWithStreamingAndStylelessDocument()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("large.log"));
        const QByteArray content = makeLineBasedContent(PerformanceConstants::LargeFileSize + 1024);
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(content), qint64(content.size()));
        }

        EditorPane pane;
        DocumentController controller(&pane, false);
        controller.openFile(path);

        CustomEditor *editor = controller.pane()->editorAt(0);
        QVERIFY(editor);

        QSignalSpy textSpy(editor, &CustomEditor::textChanged);
        QSignalSpy linesSpy(editor, &CustomEditor::linesChanged);
        QSignalSpy openedSpy(&controller, &DocumentController::fileOpened);

        QTRY_COMPARE_WITH_TIMEOUT(openedSpy.count(), 1, 30000);

        QCOMPARE(textSpy.count(), 1);
        QCOMPARE(linesSpy.count(), 1);
        QCOMPARE(editor->length(), content.size());
        QVERIFY(editor->isLargeFileMode());
        QVERIFY((editor->SendScintilla(QsciScintillaBase::SCI_GETDOCUMENTOPTIONS)
                 & QsciScintillaBase::SC_DOCUMENTOPTION_STYLES_NONE) != 0);
        QVERIFY(editor->lexer() == nullptr);
        QCOMPARE(editor->SendScintilla(QsciScintillaBase::SCI_GETMODIFY), long(0));
        QVERIFY(!editor->isReadOnly());

        // Spot-check content at the beginning, middle and end.
        QVERIFY(editor->text(0).startsWith(QStringLiteral("line 0 ")));
        const int middleLine = editor->lines() / 2;
        QVERIFY(!editor->text(middleLine).isEmpty());
        QVERIFY(editor->findFirst(QStringLiteral("abcdefghijklmnopqrstuvwxyz"),
                                  false, false, false, true, true, 0, 0));
    }

    void controllerStreamsCrlfLargeFileAndNormalizes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("crlf.txt"));
        QByteArray content;
        while (content.size() < PerformanceConstants::LargeFileSize + 1024)
            content.append("windows style line\r\n");
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(content), qint64(content.size()));
        }

        EditorPane pane;
        DocumentController controller(&pane, false);
        controller.openFile(path);

        CustomEditor *editor = controller.pane()->editorAt(0);
        QSignalSpy openedSpy(&controller, &DocumentController::fileOpened);
        QTRY_COMPARE_WITH_TIMEOUT(openedSpy.count(), 1, 30000);

        QCOMPARE(editor->documentFormat().lineEnding, DocumentLineEnding::CrLf);
        // The trailing \r\n pairs were normalized to \n in the buffer.
        QCOMPARE(editor->text(0), QStringLiteral("windows style line\n"));
    }

    void controllerClosingTabDuringStreamingLoadDoesNotCrash()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("large.txt"));
        const QByteArray content = makeLineBasedContent(32 * 1024 * 1024);
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(content), qint64(content.size()));
        }

        EditorPane pane;
        DocumentController controller(&pane, false);
        controller.openFile(path);
        controller.requestClose(0);

        QTRY_COMPARE_WITH_TIMEOUT(controller.pane()->count(), 0, 15000);
        QVERIFY(true);
    }
};

QTEST_MAIN(StreamingDocumentLoaderTest)
#include "streamingdocumentloader_test.moc"
