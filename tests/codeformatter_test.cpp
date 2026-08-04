#include <QtTest>

#include "tool/format/codeformatter.h"

class CodeFormatterTest : public QObject
{
    Q_OBJECT

private slots:
    void reportsSupportedFileTypes();
    void formatsJsonAndKeepsMemberOrder();
    void rejectsInvalidJson();
    void formatsJsonLinesOneRecordPerLine();
    void formatsXmlAndPreservesMixedContent();
    void doesNotInventAnXmlDeclaration();
    void rejectsMalformedXml();
    void preservesXmlSpacePreserveElements();
    void formatsCss();
    void keepsCssUrlsAndLineCommentsIntact();
    void formatsMarkdown();
    void leavesEmptyMarkdownAlone();
    void honoursIndentAndLineEndingOptions();
    void formattingIsIdempotent();
    void leavesFormattedDocumentsUnchanged();
};

namespace {

CodeFormatter::Options defaultOptions()
{
    CodeFormatter::Options options;
    options.indentWidth = 2;
    options.useTabs = false;
    options.eol = QStringLiteral("\n");
    return options;
}

} // namespace

void CodeFormatterTest::reportsSupportedFileTypes()
{
    QVERIFY(CodeFormatter::canFormat(QStringLiteral("/tmp/a.json")));
    QVERIFY(CodeFormatter::canFormat(QStringLiteral("/tmp/a.jsonl")));
    QVERIFY(CodeFormatter::canFormat(QStringLiteral("/tmp/a.xml")));
    QVERIFY(CodeFormatter::canFormat(QStringLiteral("/tmp/a.md")));
    QVERIFY(CodeFormatter::canFormat(QStringLiteral("/tmp/a.css")));

    QCOMPARE(CodeFormatter::formatterName(QStringLiteral("/tmp/a.json")), QStringLiteral("JSON"));
    QVERIFY(!CodeFormatter::canFormat(QString()));
    QVERIFY(!CodeFormatter::canFormat(QStringLiteral("/tmp/notes.txt")));
    QVERIFY(!CodeFormatter::canFormat(QStringLiteral("/tmp/script.py")));

    // C-family files are formattable exactly when clang-format is installed.
    const bool clangFormatAvailable = !CodeFormatter::clangFormatPath().isEmpty();
    QCOMPARE(CodeFormatter::canFormat(QStringLiteral("/tmp/a.cpp")), clangFormatAvailable);
}

void CodeFormatterTest::formatsJsonAndKeepsMemberOrder()
{
    const QString source = QStringLiteral(
        R"({"zebra":1,"alpha":{"nested":[1,2,{"x":null}],"empty":{}},"flag":true})");

    const CodeFormatter::Result result =
        CodeFormatter::format(source, QStringLiteral("/tmp/a.json"), defaultOptions());
    QVERIFY2(result.ok, qPrintable(result.error));

    const QString expected = QStringLiteral(
        "{\n"
        "  \"zebra\": 1,\n"
        "  \"alpha\": {\n"
        "    \"nested\": [\n"
        "      1,\n"
        "      2,\n"
        "      {\n"
        "        \"x\": null\n"
        "      }\n"
        "    ],\n"
        "    \"empty\": {}\n"
        "  },\n"
        "  \"flag\": true\n"
        "}\n");
    QCOMPARE(result.text, expected);
}

void CodeFormatterTest::rejectsInvalidJson()
{
    const CodeFormatter::Result result = CodeFormatter::format(QStringLiteral("{\"a\": }"),
                                                               QStringLiteral("/tmp/a.json"),
                                                               defaultOptions());
    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.text.isEmpty());
}

void CodeFormatterTest::formatsJsonLinesOneRecordPerLine()
{
    const QString source = QStringLiteral("{\"a\":1,\"b\":[1,   2]}\n\n  {\"c\":\"x\"}  \n");
    const CodeFormatter::Result result =
        CodeFormatter::format(source, QStringLiteral("/tmp/a.jsonl"), defaultOptions());
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.text, QStringLiteral("{ \"a\": 1, \"b\": [ 1, 2 ] }\n{ \"c\": \"x\" }\n"));
}

void CodeFormatterTest::formatsXmlAndPreservesMixedContent()
{
    const QString source = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<root a=\"1\"><child/><para>text <b>bold</b> tail</para>"
        "<!-- note --><empty></empty></root>");

    const CodeFormatter::Result result =
        CodeFormatter::format(source, QStringLiteral("/tmp/a.xml"), defaultOptions());
    QVERIFY2(result.ok, qPrintable(result.error));

    const QString expected = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<root a=\"1\">\n"
        "  <child/>\n"
        "  <para>text <b>bold</b> tail</para>\n"
        "  <!-- note -->\n"
        "  <empty/>\n"
        "</root>\n");
    QCOMPARE(result.text, expected);
}

void CodeFormatterTest::doesNotInventAnXmlDeclaration()
{
    const CodeFormatter::Result result = CodeFormatter::format(QStringLiteral("<a><b>1</b>\n</a>"),
                                                              QStringLiteral("/tmp/a.xml"),
                                                              defaultOptions());
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.text, QStringLiteral("<a>\n  <b>1</b>\n</a>\n"));
}

void CodeFormatterTest::rejectsMalformedXml()
{
    const CodeFormatter::Result result = CodeFormatter::format(QStringLiteral("<root><a></root>"),
                                                               QStringLiteral("/tmp/a.xml"),
                                                               defaultOptions());
    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
}

void CodeFormatterTest::preservesXmlSpacePreserveElements()
{
    const QString source = QStringLiteral(
        "<root><pre xml:space=\"preserve\"><line>  a  </line>\n  <line>b</line></pre></root>");

    const CodeFormatter::Result result =
        CodeFormatter::format(source, QStringLiteral("/tmp/a.xml"), defaultOptions());
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.text,
             QStringLiteral("<root>\n"
                            "  <pre xml:space=\"preserve\"><line>  a  </line>\n"
                            "  <line>b</line></pre>\n"
                            "</root>\n"));
}

void CodeFormatterTest::formatsCss()
{
    const QString source = QStringLiteral(
        "/* header */\n"
        "a:hover,b{color:red;background:url( x.png )}\n"
        "@media (max-width:600px){.c{margin:0}}\n");

    const CodeFormatter::Result result =
        CodeFormatter::format(source, QStringLiteral("/tmp/a.css"), defaultOptions());
    QVERIFY2(result.ok, qPrintable(result.error));

    const QString expected = QStringLiteral(
        "/* header */\n"
        "a:hover,b {\n"
        "  color: red;\n"
        "  background: url( x.png );\n"
        "}\n"
        "\n"
        "@media (max-width:600px) {\n"
        "  .c {\n"
        "    margin: 0;\n"
        "  }\n"
        "}\n");
    QCOMPARE(result.text, expected);
}

// A data URI carries both ';' and ':' inside url(), which must not be mistaken
// for a declaration terminator or a property separator.
void CodeFormatterTest::keepsCssUrlsAndLineCommentsIntact()
{
    const QString source = QStringLiteral(
        "// theme\n"
        ".a{background:url(data:image/png;base64,AAA=) no-repeat;color:red}\n");

    const CodeFormatter::Result result =
        CodeFormatter::format(source, QStringLiteral("/tmp/a.css"), defaultOptions());
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.text,
             QStringLiteral("// theme\n"
                            ".a {\n"
                            "  background: url(data:image/png;base64,AAA=) no-repeat;\n"
                            "  color: red;\n"
                            "}\n"));
}

void CodeFormatterTest::formatsMarkdown()
{
    const QString source = QStringLiteral(
        "Title\n"
        "=====\n"
        "\n"
        "*  item one\n"
        "*  item two\n"
        "\n"
        "| a | b |\n"
        "| - | - |\n"
        "| 1 | 2 |\n");

    const CodeFormatter::Result result =
        CodeFormatter::format(source, QStringLiteral("/tmp/a.md"), defaultOptions());
    QVERIFY2(result.ok, qPrintable(result.error));

    // Setext headings become ATX and list markers are normalised, while the GFM
    // table survives the round trip.
    QVERIFY(result.text.startsWith(QStringLiteral("# Title")));
    QVERIFY(result.text.contains(QStringLiteral("- item one")));
    QVERIFY(result.text.contains(QStringLiteral("| a | b |")));
    QVERIFY(result.text.contains(QStringLiteral("| 1 | 2 |")));
}

void CodeFormatterTest::leavesEmptyMarkdownAlone()
{
    const CodeFormatter::Result result = CodeFormatter::format(QStringLiteral("\n\n"),
                                                              QStringLiteral("/tmp/a.md"),
                                                              defaultOptions());
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.text, QStringLiteral("\n\n"));
}

void CodeFormatterTest::honoursIndentAndLineEndingOptions()
{
    CodeFormatter::Options options;
    options.indentWidth = 4;
    options.useTabs = true;
    options.eol = QStringLiteral("\r\n");

    const CodeFormatter::Result result =
        CodeFormatter::format(QStringLiteral("{\"a\":1}"), QStringLiteral("/tmp/a.json"), options);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.text, QStringLiteral("{\r\n\t\"a\": 1\r\n}\r\n"));
}

// Formatting an already-formatted document must be a no-op; an unstable
// formatter would keep marking files modified every time it runs.
void CodeFormatterTest::formattingIsIdempotent()
{
    const QVector<QPair<QString, QString>> samples = {
        { QStringLiteral("/tmp/a.json"),
          QStringLiteral(R"({"a":[1,{"b":"x\"y"},[]],"c":{},"d":-1.5e+3})") },
        { QStringLiteral("/tmp/a.jsonl"),
          QStringLiteral("{\"a\":1}\n{\"b\":[1,2]}\n") },
        { QStringLiteral("/tmp/a.xml"),
          QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                         "<ui version=\"4.0\"><class>MainWindow</class>"
                         "<widget class=\"QMainWindow\" name=\"MainWindow\">"
                         "<property name=\"geometry\"><rect><x>0</x><y>0</y></rect></property>"
                         "<!-- a note --><para>text <b>bold</b> tail</para>"
                         "<data><![CDATA[raw <not markup>]]></data>"
                         "</widget></ui>") },
        { QStringLiteral("/tmp/a.css"),
          QStringLiteral("/* c */ a,b{color:red;margin:0}"
                         "@media print{.x{background:url( a.png );color:blue}}") },
        { QStringLiteral("/tmp/a.md"),
          QStringLiteral("Title\n=====\n\n*  a\n*  b\n\n| a | b |\n| - | - |\n| 1 | 2 |\n\n"
                         "```cpp\nint x;\n```\n\n> quote\n") },
    };

    for (const auto &sample : samples)
    {
        const CodeFormatter::Result first =
            CodeFormatter::format(sample.second, sample.first, defaultOptions());
        QVERIFY2(first.ok, qPrintable(sample.first + QStringLiteral(": ") + first.error));
        const CodeFormatter::Result second =
            CodeFormatter::format(first.text, sample.first, defaultOptions());
        QVERIFY2(second.ok, qPrintable(sample.first + QStringLiteral(": ") + second.error));
        QVERIFY2(second.text == first.text, qPrintable(sample.first));
    }
}

void CodeFormatterTest::leavesFormattedDocumentsUnchanged()
{
    const QString formatted = QStringLiteral("{\n  \"a\": [\n    1\n  ]\n}\n");
    const CodeFormatter::Result result =
        CodeFormatter::format(formatted, QStringLiteral("/tmp/a.json"), defaultOptions());
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.text, formatted);
}

QTEST_MAIN(CodeFormatterTest)
#include "codeformatter_test.moc"
