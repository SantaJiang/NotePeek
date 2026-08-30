#include "editor/documentstreamdecoder.h"
#include "editor/documentformat.h"

#include <QByteArray>
#include <QRandomGenerator>
#include <QtTest>

namespace {

struct StreamOutcome
{
    bool success = false;
    QByteArray output;
    DocumentFormat format;
    QString error;
};

StreamOutcome streamDecode(const QByteArray &input,
                           const QList<qsizetype> &chunkSizes,
                           bool hasOverride = false,
                           DocumentEncoding encoding = DocumentEncoding::Utf8)
{
    DocumentStreamDecoder decoder;
    if (hasOverride)
        decoder.setEncodingOverride(encoding);

    StreamOutcome outcome;
    qsizetype pos = 0;
    int chunkIndex = 0;
    while (pos < input.size())
    {
        qsizetype n = chunkSizes.isEmpty() ? input.size() : chunkSizes.at(chunkIndex % chunkSizes.size());
        n = qMin(n, input.size() - pos);
        if (!decoder.accept(input.constData() + pos, n, &outcome.output, &outcome.error))
            return outcome;
        pos += n;
        ++chunkIndex;
    }
    outcome.success = decoder.finish(&outcome.output, &outcome.error);
    outcome.format = decoder.metadata().format;
    return outcome;
}

QByteArray utf16(const QList<quint16> &units, bool littleEndian)
{
    QByteArray bytes;
    bytes.reserve(units.size() * 2);
    for (quint16 unit : units)
    {
        if (littleEndian)
        {
            bytes.append(char(unit & 0xff));
            bytes.append(char((unit >> 8) & 0xff));
        }
        else
        {
            bytes.append(char((unit >> 8) & 0xff));
            bytes.append(char(unit & 0xff));
        }
    }
    return bytes;
}

} // namespace

class DocumentStreamDecoderTest : public QObject
{
    Q_OBJECT

private slots:
    void utf8MultiByteCharactersSplitAcrossChunks()
    {
        const QByteArray input = "\xc3\xa9"             // é (2 bytes)
                                 "\xe4\xb8\xad"         // 中 (3 bytes)
                                 "\xf0\x9f\x98\x80";    // 😀 (4 bytes)
        const QByteArray expected = input;

        for (int chunkSize = 1; chunkSize <= 4; ++chunkSize)
        {
            const StreamOutcome outcome = streamDecode(input, {chunkSize});
            QVERIFY2(outcome.success, qPrintable(outcome.error));
            QCOMPARE(outcome.output, expected);
            QCOMPARE(outcome.format.encoding, DocumentEncoding::Utf8);
        }
    }

    void utf8BomSplitAcrossChunks()
    {
        const QByteArray payload = "hello\n";
        const QByteArray input = QByteArray("\xef\xbb\xbf") + payload;
        for (int chunkSize = 1; chunkSize <= 3; ++chunkSize)
        {
            const StreamOutcome outcome = streamDecode(input, {chunkSize});
            QVERIFY2(outcome.success, qPrintable(outcome.error));
            QCOMPARE(outcome.output, payload);
            QCOMPARE(outcome.format.encoding, DocumentEncoding::Utf8Bom);
        }
    }

    void utf16BomAndSurrogatePairsSplitAcrossChunks()
    {
        // "A😀B" in UTF-16 LE: 0x41, 0xD83D, 0xDE00, 0x42.
        const QList<quint16> units = {0x0041, 0xd83d, 0xde00, 0x0042};
        const QByteArray bom = QByteArray("\xff\xfe");
        const QByteArray input = bom + utf16(units, true);
        const QByteArray expected = "A\xf0\x9f\x98\x80" "B";

        for (int chunkSize = 1; chunkSize <= 4; ++chunkSize)
        {
            const StreamOutcome outcome = streamDecode(input, {chunkSize});
            QVERIFY2(outcome.success, qPrintable(outcome.error));
            QCOMPARE(outcome.output, expected);
            QCOMPARE(outcome.format.encoding, DocumentEncoding::Utf16LittleEndian);
        }

        const QByteArray bigEndian = QByteArray("\xfe\xff") + utf16(units, false);
        const StreamOutcome bigOutcome = streamDecode(bigEndian, {3});
        QVERIFY(bigOutcome.success);
        QCOMPARE(bigOutcome.output, expected);
        QCOMPARE(bigOutcome.format.encoding, DocumentEncoding::Utf16BigEndian);
    }

    void crlfAndCrNormalizedAcrossChunks()
    {
        // "a\r\nb\rc\nd"
        const QByteArray input = "a\r\nb\rc\nd";
        const QByteArray expected = "a\nb\nc\nd";
        for (int chunkSize = 1; chunkSize <= 3; ++chunkSize)
        {
            const StreamOutcome outcome = streamDecode(input, {chunkSize});
            QVERIFY(outcome.success);
            QCOMPARE(outcome.output, expected);
            QCOMPARE(outcome.format.lineEnding, DocumentLineEnding::Mixed);
        }

        const StreamOutcome crlfOnly = streamDecode("a\r\nb\r\n", {2});
        QCOMPARE(crlfOnly.output, QByteArray("a\nb\n"));
        QCOMPARE(crlfOnly.format.lineEnding, DocumentLineEnding::CrLf);

        const StreamOutcome crOnly = streamDecode("a\rb\rc", {2});
        QCOMPARE(crOnly.output, QByteArray("a\nb\nc"));
        QCOMPARE(crOnly.format.lineEnding, DocumentLineEnding::Cr);

        const StreamOutcome lfOnly = streamDecode("a\nb\nc", {2});
        QCOMPARE(lfOnly.output, QByteArray("a\nb\nc"));
        QCOMPARE(lfOnly.format.lineEnding, DocumentLineEnding::Lf);
    }

    void trailingCrAtEndOfInput()
    {
        const StreamOutcome outcome = streamDecode("line\r", {2});
        QVERIFY(outcome.success);
        QCOMPARE(outcome.output, QByteArray("line\n"));
        QCOMPARE(outcome.format.lineEnding, DocumentLineEnding::Cr);
    }

    void emptyAndBomOnlyFiles()
    {
        const StreamOutcome empty = streamDecode(QByteArray(), {2});
        QVERIFY(empty.success);
        QCOMPARE(empty.output, QByteArray());
        QCOMPARE(empty.format.encoding, DocumentEncoding::Utf8);
        QCOMPARE(empty.format.lineEnding, DocumentFormatCodec::defaultFormat().lineEnding);

        const StreamOutcome utf8BomOnly = streamDecode("\xef\xbb\xbf", {1});
        QVERIFY(utf8BomOnly.success);
        QCOMPARE(utf8BomOnly.output, QByteArray());
        QCOMPARE(utf8BomOnly.format.encoding, DocumentEncoding::Utf8Bom);

        const StreamOutcome utf16BomOnly = streamDecode("\xff\xfe", {1});
        QVERIFY(utf16BomOnly.success);
        QCOMPARE(utf16BomOnly.output, QByteArray());
        QCOMPARE(utf16BomOnly.format.encoding, DocumentEncoding::Utf16LittleEndian);
    }

    void invalidUtf8Rejected()
    {
        // Invalid continuation byte.
        QVERIFY(!streamDecode("\xc3\x28", {4}).success);
        // Overlong encoding of '/'.
        QVERIFY(!streamDecode("\xc0\xaf", {4}).success);
        // Surrogate range encoded in UTF-8.
        QVERIFY(!streamDecode("\xed\xa0\x80", {4}).success);
        // Codepoint above U+10FFFF.
        QVERIFY(!streamDecode("\xf4\x90\x80\x80", {4}).success);
        // Truncated at EOF.
        QVERIFY(!streamDecode("\xe4\xb8", {4}).success);
    }

    void invalidUtf16Rejected()
    {
        // Odd number of payload bytes.
        QVERIFY(!streamDecode(QByteArray("\xff\xfe") + QByteArray("A", 1), {3}).success);
        // Dangling high surrogate at EOF.
        QVERIFY(!streamDecode(QByteArray("\xff\xfe") + utf16({0xd83d}, true), {2}).success);
        // Lone low surrogate.
        QVERIFY(!streamDecode(QByteArray("\xff\xfe") + utf16({0xde00}, true), {2}).success);
    }

    void latin1OverrideExpandsToUtf8()
    {
        const QByteArray input = QByteArray("caf\xe9"); // "café" in Latin-1.
        const StreamOutcome outcome = streamDecode(input, {2}, true, DocumentEncoding::Latin1);
        QVERIFY(outcome.success);
        QCOMPARE(outcome.output, QByteArray("caf\xc3\xa9"));
        QCOMPARE(outcome.format.encoding, DocumentEncoding::Latin1);
    }

    void forcedUtf16OverrideConsumesMatchingBom()
    {
        const QByteArray input = QByteArray("\xff\xfe") + utf16({0x0041, 0x0042}, true);
        const StreamOutcome outcome = streamDecode(input, {1}, true, DocumentEncoding::Utf16LittleEndian);
        QVERIFY(outcome.success);
        QCOMPARE(outcome.output, QByteArray("AB"));
        QCOMPARE(outcome.format.encoding, DocumentEncoding::Utf16LittleEndian);
    }

    void differentialAgainstWholeFileDecoder()
    {
        // Structured valid inputs covering every encoding.
        QList<QByteArray> corpus;
        corpus << QByteArray()
               << QByteArray("plain ascii\nwith\nlines\n")
               << QByteArray("caf\xc3\xa9 \xe4\xb8\xad\xe6\x96\x87 \xf0\x9f\x98\x80\n")
               << QByteArray("\xef\xbb\xbf") + "bom utf8\n"
               << QByteArray("mixed\r\nline\rhere\n")
               << QByteArray("\xff\xfe") + utf16({0x0041, 0xd83d, 0xde00, 0x0d, 0x000a}, true)
               << QByteArray("\xfe\xff") + utf16({0x0041, 0x0042, 0x000d, 0x000a}, false)
               << QByteArray("trailing cr\r")
               << QByteArray("\xc3\xa9") // incomplete at 1 byte
               << QByteArray("\xc3") // truncated
               << QByteArray("\xff\xfe\x41") // UTF-16 odd payload
               << QByteArray("\xff\xfe") + utf16({0xd83d}, true); // dangling high surrogate

        for (const QByteArray &input : corpus)
        {
            QByteArray oldOutput;
            DocumentFormat oldFormat;
            QString oldError;
            const bool oldOk = DocumentFormatCodec::decodeToEditorBytes(
                input, &oldOutput, &oldFormat, &oldError);

            const StreamOutcome outcome = streamDecode(input, {1, 3, 2, 5});
            QCOMPARE(outcome.success, oldOk);
            if (oldOk)
            {
                QCOMPARE(outcome.output, oldOutput);
                QCOMPARE(outcome.format.encoding, oldFormat.encoding);
                QCOMPARE(outcome.format.lineEnding, oldFormat.lineEnding);
            }
        }

        // Random byte strings, fixed seed for reproducibility.
        QRandomGenerator rng(0x5eedcafe);
        for (int iteration = 0; iteration < 4000; ++iteration)
        {
            QByteArray random;
            const int length = int(rng.bounded(64));
            for (int i = 0; i < length; ++i)
                random.append(char(rng.bounded(256)));

            QByteArray oldOutput;
            DocumentFormat oldFormat;
            QString oldError;
            const bool oldOk = DocumentFormatCodec::decodeToEditorBytes(
                random, &oldOutput, &oldFormat, &oldError);

            const QList<qsizetype> chunkPattern = {qsizetype(1 + rng.bounded(6))};
            const StreamOutcome outcome = streamDecode(random, chunkPattern);
            QCOMPARE(outcome.success, oldOk);
            if (oldOk)
            {
                QCOMPARE(outcome.output, oldOutput);
                QCOMPARE(outcome.format.encoding, oldFormat.encoding);
                QCOMPARE(outcome.format.lineEnding, oldFormat.lineEnding);
            }
        }
    }
};

QTEST_MAIN(DocumentStreamDecoderTest)
#include "documentstreamdecoder_test.moc"
