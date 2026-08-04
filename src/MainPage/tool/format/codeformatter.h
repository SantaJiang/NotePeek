#ifndef CODEFORMATTER_H
#define CODEFORMATTER_H

#include <QCoreApplication>
#include <QString>

// Whole-document code formatting for the languages NotePeek can reformat
// safely.
//
// Most languages are handled in-process so a stock installation can format
// without any external program: JSON/JSON Lines, XML, Markdown (through the
// vendored cmark-gfm) and CSS.  C-family languages have no faithful built-in
// implementation, so they are delegated to clang-format when the user has it on
// PATH; canFormat() reports false for them otherwise, which keeps the menu
// entry honest about what will happen when it is clicked.
class CodeFormatter
{
    Q_DECLARE_TR_FUNCTIONS(CodeFormatter)

public:
    struct Options
    {
        // Honoured by the JSON, XML and CSS formatters.  Markdown indentation is
        // fixed by the CommonMark spelling cmark-gfm emits.
        int indentWidth = 4;
        bool useTabs = false;
        // Line ending written between output lines.  Formatters normalise the
        // whole document to it.
        QString eol = QStringLiteral("\n");
    };

    struct Result
    {
        bool ok = false;
        QString text;
        // Human-readable reason, set when ok is false.
        QString error;
    };

    // True when format() would do real work for this file.  Includes the
    // clang-format lookup for C-family files.
    static bool canFormat(const QString &filePath);

    // Display name of the formatter that handles this file ("JSON",
    // "clang-format", ...), or an empty string when the file has none.
    static QString formatterName(const QString &filePath);

    static Result format(const QString &text, const QString &filePath, const Options &options);

    // Absolute path of the clang-format binary, or an empty string when it is
    // not installed.  The lookup is cached for the lifetime of the process, so
    // installing clang-format takes effect at the next launch.
    static QString clangFormatPath();
};

#endif // CODEFORMATTER_H
