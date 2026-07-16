#include "editor/customeditor.h"
#include "editor/documentformat.h"
#include "editor/documentpersistenceservice.h"
#include "editor/documentsavecoordinator.h"

#include <QElapsedTimer>
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
};

QTEST_MAIN(PerformanceTest)
#include "performance_test.moc"
