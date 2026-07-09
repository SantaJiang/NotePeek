#include "editor/documentio.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class DocumentIOTest : public QObject
{
    Q_OBJECT

private slots:
    void loadAndSaveUtf8Text()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString path = tempDir.filePath(QStringLiteral("note.md"));
        QString error;
        QVERIFY2(DocumentIO::saveTextFile(path, QStringLiteral("# Title\nhello"), &error),
                 qPrintable(error));

        QString content;
        QVERIFY2(DocumentIO::loadTextFile(path, &content, &error), qPrintable(error));
        QCOMPARE(content, QStringLiteral("# Title\nhello"));
    }

    void saveUtf8BytesPreservesContent()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString path = tempDir.filePath(QStringLiteral("bytes.txt"));
        QString error;
        const QByteArray bytes("alpha\nbeta\n");
        QVERIFY2(DocumentIO::saveUtf8File(path, bytes, &error), qPrintable(error));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), bytes);
    }
};

QTEST_MAIN(DocumentIOTest)
#include "documentio_test.moc"
