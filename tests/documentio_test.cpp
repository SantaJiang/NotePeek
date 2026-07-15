#include "editor/documentio.h"
#include "editor/documentpersistenceservice.h"

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

    void asyncSavesAreFifoAcrossServices()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString path = tempDir.filePath(QStringLiteral("shared.txt"));
        DocumentPersistenceService leftService;
        DocumentPersistenceService rightService;
        QVector<quint64> completionOrder;

        connect(&leftService, &DocumentPersistenceService::fileSaved,
                this, [&completionOrder](const DocumentSaveResult &result) {
            QVERIFY(result.success);
            completionOrder.append(result.requestId);
        });
        connect(&rightService, &DocumentPersistenceService::fileSaved,
                this, [&completionOrder](const DocumentSaveResult &result) {
            QVERIFY(result.success);
            completionOrder.append(result.requestId);
        });

        leftService.saveFileAsync({1, path, QByteArray("older")});
        rightService.saveFileAsync({2, path, QByteArray("newest")});
        leftService.waitForPendingSavePath(path);

        QTRY_COMPARE(completionOrder.size(), 2);
        QCOMPARE(completionOrder, QVector<quint64>({1, 2}));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("newest"));
    }

    void atomicSavePreservesSymbolicLinkTarget()
    {
#ifdef Q_OS_UNIX
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString targetPath = tempDir.filePath(QStringLiteral("target.txt"));
        const QString linkPath = tempDir.filePath(QStringLiteral("link.txt"));

        QString error;
        QVERIFY2(DocumentIO::saveUtf8File(targetPath, QByteArray("old"), &error),
                 qPrintable(error));
        QVERIFY(QFile::link(targetPath, linkPath));
        QVERIFY(QFileInfo(linkPath).isSymLink());

        QVERIFY2(DocumentIO::saveUtf8File(linkPath, QByteArray("new"), &error),
                 qPrintable(error));
        QVERIFY(QFileInfo(linkPath).isSymLink());

        QFile target(targetPath);
        QVERIFY(target.open(QIODevice::ReadOnly));
        QCOMPARE(target.readAll(), QByteArray("new"));
#else
        QSKIP("Creating symbolic links requires platform-specific privileges.");
#endif
    }

    void rapidSavesCoalesceQueuedSnapshots()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("coalesced.txt"));

        DocumentPersistenceService service;
        QHash<quint64, bool> completions;
        connect(&service, &DocumentPersistenceService::fileSaved,
                this, [&completions](const DocumentSaveResult &result) {
            QVERIFY(result.success);
            completions.insert(result.requestId, result.superseded);
        });

        constexpr quintptr documentId = 42;
        service.saveFileAsync({10, path, QByteArray("active"), documentId});
        service.saveFileAsync({11, path, QByteArray("obsolete"), documentId});
        service.saveFileAsync({12, path, QByteArray("latest"), documentId});
        service.waitForPendingSavePath(path);

        QTRY_COMPARE(completions.size(), 3);
        QCOMPARE(completions.value(10), false);
        QCOMPARE(completions.value(11), true);
        QCOMPARE(completions.value(12), false);

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("latest"));
    }
};

QTEST_MAIN(DocumentIOTest)
#include "documentio_test.moc"
