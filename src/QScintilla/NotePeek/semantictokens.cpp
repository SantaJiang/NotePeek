// Semantic-token API for the NotePeek QScintilla fork.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#include "NotePeek/semantictokens.h"

#include <QRegularExpression>
#include <QSet>
#include <QElapsedTimer>

namespace {

using NotePeek::SemanticEntryState;

// Syntax that a language uses for comments and multi-line strings.  Python and
// Starlark share `#` line comments and triple-quoted strings; the C family
// shares `//` line comments and `/* */` blocks.  Mixing the two sets silently
// corrupts the mask: `total = a // b` in Python is a division, not a comment.
struct MaskDialect
{
    bool hashLineComments = false;
    bool slashLineComments = false;
    bool slashStarBlockComments = false;
    bool tripleQuotedStrings = false;
    bool backtickStrings = false;
};

bool matchesAt(const QString &line, int index, QLatin1String needle)
{
    return index >= 0 && QStringView(line).mid(index).startsWith(needle);
}

void blankRange(QString &masked, int from, int count)
{
    for (int i = from; i < from + count && i < masked.size(); ++i)
        masked[i] = QLatin1Char(' ');
}

// Closes a window that began inside a multi-line string.  The caller can only
// tell us that "a string was open here", not which flavour, so both triple
// quotes are accepted.  `)"` is only a terminator where raw strings exist:
// accepting it in Python would end a docstring early on text like
// `"""call(x)" is a thing"""`.
bool closesMultiLineString(const QString &line, int index, const MaskDialect &dialect, int *width)
{
    if (dialect.tripleQuotedStrings
        && (matchesAt(line, index, QLatin1String("\"\"\"")) || matchesAt(line, index, QLatin1String("'''"))))
    {
        *width = 3;
        return true;
    }
    if (!dialect.tripleQuotedStrings && matchesAt(line, index, QLatin1String(")\"")))
    {
        *width = 2;
        return true;
    }
    return false;
}

// Hide strings and comments while preserving UTF-16 offsets. This classifier
// is deliberately conservative: lexical styles remain the fallback whenever
// a name cannot be identified safely.
QString maskNonCode(const QString &line, const MaskDialect &dialect, SemanticEntryState &state)
{
    QString masked = line;
    QChar quote;
    bool escaped = false;
    for (int i = 0; i < line.size(); ++i)
    {
        const QChar current = line.at(i);
        const QChar next = i + 1 < line.size() ? line.at(i + 1) : QChar();

        if (state == SemanticEntryState::BlockComment)
        {
            masked[i] = QLatin1Char(' ');
            if (current == QLatin1Char('*') && next == QLatin1Char('/'))
            {
                masked[++i] = QLatin1Char(' ');
                state = SemanticEntryState::Code;
            }
            continue;
        }
        if (state == SemanticEntryState::MultiLineString)
        {
            int width = 0;
            if (closesMultiLineString(line, i, dialect, &width))
            {
                blankRange(masked, i, width);
                i += width - 1;
                state = SemanticEntryState::Code;
            }
            else
            {
                masked[i] = QLatin1Char(' ');
            }
            continue;
        }
        if (!quote.isNull())
        {
            masked[i] = QLatin1Char(' ');
            if (!escaped && current == quote)
                quote = QChar();
            escaped = !escaped && current == QLatin1Char('\\');
            continue;
        }

        if (dialect.tripleQuotedStrings
            && (matchesAt(line, i, QLatin1String("\"\"\"")) || matchesAt(line, i, QLatin1String("'''"))))
        {
            blankRange(masked, i, 3);
            i += 2;
            state = SemanticEntryState::MultiLineString;
        }
        else if (dialect.slashStarBlockComments && current == QLatin1Char('/') && next == QLatin1Char('*'))
        {
            blankRange(masked, i, 2);
            ++i;
            state = SemanticEntryState::BlockComment;
        }
        else if (dialect.slashLineComments && current == QLatin1Char('/') && next == QLatin1Char('/'))
        {
            blankRange(masked, i, masked.size() - i);
            break;
        }
        else if (dialect.hashLineComments && current == QLatin1Char('#'))
        {
            blankRange(masked, i, masked.size() - i);
            break;
        }
        else if (current == QLatin1Char('\'') || current == QLatin1Char('"')
                 || (dialect.backtickStrings && current == QLatin1Char('`')))
        {
            quote = current;
            masked[i] = QLatin1Char(' ');
            escaped = false;
        }
    }
    // An unterminated single-line string does not leak into the next line;
    // only the explicit multi-line states do.
    return masked;
}

// Several patterns match the same name.  Resolving that by letting whichever
// indicator is filled last win makes the palette depend on statement order in
// this file, so overlaps are resolved explicitly instead: the first rule to
// claim a column keeps it, and rules are tried most-specific first.
struct PatternRule
{
    const QRegularExpression *pattern;
    NotePeek::SyntaxRole role;
    const QSet<QString> *excluded;
};

void collectLineTokens(QVector<NotePeek::SemanticToken> &tokens, int lineNumber,
                       const QString &line, const QVector<PatternRule> &rules,
                       QVector<bool> &claimed)
{
    claimed.fill(false, line.size());
    for (const PatternRule &rule : rules)
    {
        QRegularExpressionMatchIterator matches = rule.pattern->globalMatch(line);
        while (matches.hasNext())
        {
            const QRegularExpressionMatch match = matches.next();
            const int start = int(match.capturedStart(1));
            const int length = int(match.capturedLength(1));
            if (start < 0 || length <= 0)
                continue;
            if (rule.excluded && rule.excluded->contains(match.captured(1)))
                continue;
            bool overlaps = false;
            for (int i = start; i < start + length && i < claimed.size(); ++i)
                overlaps = overlaps || claimed.at(i);
            if (overlaps)
                continue;
            for (int i = start; i < start + length && i < claimed.size(); ++i)
                claimed[i] = true;
            tokens.append({lineNumber, start, length, rule.role});
        }
    }
}

} // namespace

namespace NotePeek {

SemanticAnalysisResult BasicSemanticTokenProvider::analyze(
    const SemanticDocument &document, const SemanticAnalysisControl &control) const
{
    SemanticAnalysisResult result;
    result.revision = document.revision;
    result.firstLine = document.firstLine;
    result.exitState = document.entryState;
    QElapsedTimer timer;
    timer.start();
    if (document.text.size() > 2 * 1024 * 1024)
        return result;

    const bool pythonLike = document.languageId.compare(QLatin1String("Python"), Qt::CaseInsensitive) == 0
                            || document.languageId.compare(QLatin1String("Starlark"), Qt::CaseInsensitive) == 0;
    const bool scriptLike = document.languageId == QLatin1String("JavaScript")
                            || document.languageId == QLatin1String("QML");
    const bool codeLike = pythonLike || scriptLike || document.languageId == QLatin1String("C++")
                          || document.languageId == QLatin1String("C#")
                          || document.languageId == QLatin1String("Java")
                          || document.languageId == QLatin1String("Rust");
    if (!codeLike)
        return result;

    MaskDialect dialect;
    dialect.hashLineComments = pythonLike;
    dialect.tripleQuotedStrings = pythonLike;
    // `//` is a line comment in the C family but floor division in Python and
    // Starlark, where treating it as a comment would blank the rest of the line
    // and silently drop every token after an integer division.
    dialect.slashLineComments = !pythonLike;
    dialect.slashStarBlockComments = !pythonLike;
    dialect.backtickStrings = scriptLike;

    static const QSet<QString> controlWords = {
        QStringLiteral("if"), QStringLiteral("for"), QStringLiteral("while"),
        QStringLiteral("switch"), QStringLiteral("catch"), QStringLiteral("return"),
        QStringLiteral("sizeof"), QStringLiteral("decltype"), QStringLiteral("new"),
        QStringLiteral("def"), QStringLiteral("class"), QStringLiteral("match")
    };
    static const QRegularExpression functionPattern(QStringLiteral(R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*(?=\())"));
    static const QRegularExpression typePattern(QStringLiteral(R"(\b([A-Z][A-Za-z0-9_]*)\b)"));
    static const QRegularExpression propertyPattern(QStringLiteral(R"((?:\.|->)\s*([A-Za-z_][A-Za-z0-9_]*))"));
    static const QRegularExpression variablePattern(QStringLiteral(
        R"(\b(?:auto|bool|char|double|float|int|long|short|string|QString|var|let|const)\s+([a-z_][A-Za-z0-9_]*))"));
    static const QRegularExpression pythonFunctionPattern(QStringLiteral(R"(\bdef\s+([A-Za-z_][A-Za-z0-9_]*))"));
    static const QRegularExpression pythonClassPattern(QStringLiteral(R"(\bclass\s+([A-Za-z_][A-Za-z0-9_]*))"));
    static const QRegularExpression decoratorPattern(QStringLiteral(R"(@([A-Za-z_][A-Za-z0-9_]*))"));
    static const QRegularExpression macroPattern(QStringLiteral(R"(\b([A-Z][A-Z0-9_]{2,})\b)"));

    // Ordered most-specific first.  An explicit declaration (`def name`) beats
    // a generic shape (`name` followed by a parenthesis), which in turn beats
    // the weakest signal (any capitalised word).  See PatternRule.
    QVector<PatternRule> rules;
    if (pythonLike)
    {
        rules.append({&pythonFunctionPattern, SyntaxRole::Function, nullptr});
        rules.append({&pythonClassPattern, SyntaxRole::Type, nullptr});
        rules.append({&decoratorPattern, SyntaxRole::Attribute, nullptr});
    }
    rules.append({&macroPattern, SyntaxRole::Builtin, nullptr});
    rules.append({&variablePattern, SyntaxRole::Variable, nullptr});
    rules.append({&typePattern, SyntaxRole::Type, nullptr});
    rules.append({&functionPattern, SyntaxRole::Function, &controlWords});
    rules.append({&propertyPattern, SyntaxRole::Property, nullptr});

    SemanticEntryState state = document.entryState;
    QVector<bool> claimed;
    const QStringList lines = document.text.split(QLatin1Char('\n'));
    for (int lineNumber = 0; lineNumber < lines.size(); ++lineNumber)
    {
        if (control.cancellation && control.cancellation->load(std::memory_order_relaxed))
        {
            result.canceled = true;
            break;
        }
        if ((lineNumber & 0x0f) == 0 && control.budgetNanoseconds > 0
            && timer.nsecsElapsed() > control.budgetNanoseconds)
        {
            result.budgetExceeded = true;
            break;
        }
        const QString code = maskNonCode(lines.at(lineNumber), dialect, state);
        const int absoluteLine = document.firstLine + lineNumber;
        collectLineTokens(result.tokens, absoluteLine, code, rules, claimed);
        result.lastLine = absoluteLine;
        result.exitState = state;
    }
    result.elapsedNanoseconds = timer.nsecsElapsed();
    return result;
}

} // namespace NotePeek
