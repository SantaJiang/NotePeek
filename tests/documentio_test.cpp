#include "editor/documentio.h"
#include "editor/documentpersistenceservice.h"
#include "editor/documentpath.h"
#include "editor/documentsavecoordinator.h"

#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QtTest>

class DocumentIOTest : public QObject
{
    Q_OBJECT

private slots:
    void randomizedCodecInputsNeverCrash()
    {
        QRandomGenerator random(0x4e6f7465U);
        const QList<DocumentEncoding> encodings = DocumentFormatCodec::supportedEncodings();
        for (int iteration = 0; iteration < 2500; ++iteration)
        {
            QByteArray bytes(int(random.bounded(4097)), Qt::Uninitialized);
            for (char &byte : bytes)
                byte = char(random.generate() & 0xffU);
            if (iteration % 7 == 0)
                bytes.prepend("\xef\xbb\xbf", 3);
            else if (iteration % 11 == 0)
                bytes.prepend("\xff\xfe", 2);

            QByteArray editorBytes;
            DocumentFormat format;
            QString error;
            DocumentFormatCodec::decodeToEditorBytes(bytes, &editorBytes, &format, &error);
            for (DocumentEncoding encoding : encodings)
            {
                QString text;
                DocumentFormat forcedFormat;
                DocumentFormatCodec::decodeWithEncoding(bytes, encoding, &text,
                                                        &forcedFormat, &error);
            }
        }
    }

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

    void preservesEncodingAndLineEndings()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QList<DocumentEncoding> encodings = {
            DocumentEncoding::Utf8,
            DocumentEncoding::Utf8Bom,
            DocumentEncoding::Utf16LittleEndian,
            DocumentEncoding::Utf16BigEndian
        };
        for (DocumentEncoding encoding : encodings)
        {
            const QString path = tempDir.filePath(QStringLiteral("format-%1.txt").arg(int(encoding)));
            const DocumentFormat expected{encoding, DocumentLineEnding::CrLf};
            QString error;
            QVERIFY2(DocumentIO::saveTextFile(path,
                                              QStringLiteral("alpha\n中文😀\n"),
                                              expected,
                                              &error),
                     qPrintable(error));

            QString content;
            DocumentFormat actual;
            QVERIFY2(DocumentIO::loadTextFile(path, &content, &actual, &error), qPrintable(error));
            QCOMPARE(content, QStringLiteral("alpha\n中文😀\n"));
            QCOMPARE(actual.encoding, expected.encoding);
            QCOMPARE(actual.lineEnding, expected.lineEnding);
        }
    }

    void reopensAndRoundTripsLatin1()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("latin1.txt"));

        const QByteArray latin1Bytes("caf\xe9\n", 5);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(latin1Bytes), qint64(latin1Bytes.size()));
        file.close();

        QString content;
        DocumentFormat format;
        QString error;
        QByteArray editorContent;
        QVERIFY2(DocumentIO::loadEditorFileWithEncoding(path,
                                                        DocumentEncoding::Latin1,
                                                        &editorContent,
                                                        &format,
                                                        &error),
                 qPrintable(error));
        content = QString::fromUtf8(editorContent);
        QCOMPARE(content, QStringLiteral("café\n"));
        QCOMPARE(format.encoding, DocumentEncoding::Latin1);

        QVERIFY2(DocumentIO::saveTextFile(path, content, format, &error), qPrintable(error));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), latin1Bytes);
        file.close();
    }

    void reportsCharactersLatin1CannotStore()
    {
        QVERIFY(!DocumentFormatCodec::canRepresent(QStringLiteral("中文"), DocumentEncoding::Latin1));
        QVERIFY(DocumentFormatCodec::canRepresent(QStringLiteral("café"), DocumentEncoding::Latin1));
        // The UTF encodings always cover the whole of Unicode.
        QVERIFY(DocumentFormatCodec::canRepresent(QStringLiteral("中文😀"), DocumentEncoding::Utf8));
    }

    void rejectsInvalidTextInsteadOfReplacingBytes()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("invalid.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArray("valid\xffinvalid", 13)), qint64(13));
        file.close();

        QString content;
        QString error;
        QVERIFY(!DocumentIO::loadTextFile(path, &content, &error));
        QVERIFY(!error.isEmpty());
    }

    void rejectsMalformedUtf16Surrogates()
    {
        QString content;
        DocumentFormat format;
        QString error;
        const QByteArray loneHighSurrogate("\xff\xfe\x00\xd8\x41\x00", 6);
        QVERIFY(!DocumentFormatCodec::decode(loneHighSurrogate,
                                             &content,
                                             &format,
                                             &error));
        QVERIFY(!error.isEmpty());

        error.clear();
        const QByteArray loneLowSurrogate("\xfe\xff\xdc\x00", 4);
        QVERIFY(!DocumentFormatCodec::decode(loneLowSurrogate,
                                             &content,
                                             &format,
                                             &error));
        QVERIFY(!error.isEmpty());
    }

    void usesNativeLineEndingWhenTheFileHasNoNewline()
    {
        QString content;
        DocumentFormat format;
        QString error;
        QVERIFY2(DocumentFormatCodec::decode(QByteArray("one line"),
                                             &content,
                                             &format,
                                             &error),
                 qPrintable(error));
        QCOMPARE(format.lineEnding, DocumentFormatCodec::defaultFormat().lineEnding);
    }

    void normalizesEquivalentPaths()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("note.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        QVERIFY(DocumentPath::equivalent(path,
                                         tempDir.path() + QStringLiteral("/folder/../note.txt")));
        QVERIFY(DocumentPath::isInsideDirectory(path, tempDir.path()));
    }

    void loadsNormalizedUtf8ForTheEditor()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString path = tempDir.filePath(QStringLiteral("editor.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("alpha\r\nbeta\r\n"), qint64(13));
        file.close();

        QByteArray content;
        DocumentFormat format;
        QString error;
        QVERIFY2(DocumentIO::loadEditorFile(path, &content, &format, &error), qPrintable(error));
        QCOMPARE(content, QByteArray("alpha\nbeta\n"));
        QCOMPARE(format.lineEnding, DocumentLineEnding::CrLf);
    }

    void loadsUtf8EditorBytesWithoutChangingFormatSemantics()
    {
        struct Fixture
        {
            QByteArray input;
            QByteArray expected;
            DocumentEncoding encoding;
            DocumentLineEnding lineEnding;
        };
        const QList<Fixture> fixtures = {
            {QByteArray("alpha\nbeta\n"), QByteArray("alpha\nbeta\n"),
             DocumentEncoding::Utf8, DocumentLineEnding::Lf},
            {QByteArray("\xef\xbb\xbf" "alpha\nbeta\n", 14), QByteArray("alpha\nbeta\n"),
             DocumentEncoding::Utf8Bom, DocumentLineEnding::Lf},
            {QByteArray("alpha\rbeta\r"), QByteArray("alpha\nbeta\n"),
             DocumentEncoding::Utf8, DocumentLineEnding::Cr},
            {QByteArray("alpha\r\nbeta\n"), QByteArray("alpha\nbeta\n"),
             DocumentEncoding::Utf8, DocumentLineEnding::Mixed}
        };

        for (const Fixture &fixture : fixtures)
        {
            QByteArray editorBytes;
            DocumentFormat format;
            QString error;
            QVERIFY2(DocumentFormatCodec::decodeToEditorBytes(
                         fixture.input, &editorBytes, &format, &error),
                     qPrintable(error));
            QCOMPARE(editorBytes, fixture.expected);
            QCOMPARE(format.encoding, fixture.encoding);
            QCOMPARE(format.lineEnding, fixture.lineEnding);
        }
    }

    void directEditorDecodeRejectsInvalidUtf8()
    {
        QByteArray editorBytes;
        DocumentFormat format;
        QString error;
        QVERIFY(!DocumentFormatCodec::decodeToEditorBytes(
            QByteArray("valid\xffinvalid", 13), &editorBytes, &format, &error));
        QVERIFY(!error.isEmpty());
    }

    void asyncSavesAreFifoAcrossServices()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString path = tempDir.filePath(QStringLiteral("shared.txt"));
        DocumentSaveCoordinator coordinator;
        DocumentPersistenceService leftService(&coordinator);
        DocumentPersistenceService rightService(&coordinator);
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

        DocumentSaveCoordinator coordinator;
        DocumentPersistenceService service(&coordinator);
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
