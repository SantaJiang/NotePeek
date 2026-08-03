// Semantic-token API for the NotePeek QScintilla fork.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#include "NotePeek/semantictokens.h"

#include <QRegularExpression>
#include <QSet>
#include <QElapsedTimer>

namespace {

void appendMatches(QVector<NotePeek::SemanticToken> &tokens, int lineNumber,
                   const QString &line, const QRegularExpression &pattern,
                   int capture, NotePeek::SyntaxRole role,
                   const QSet<QString> &excluded = {})
{
    QRegularExpressionMatchIterator matches = pattern.globalMatch(line);
    while (matches.hasNext())
    {
        const QRegularExpressionMatch match = matches.next();
        const QString value = match.captured(capture);
        if (excluded.contains(value))
            continue;
        tokens.append({lineNumber, int(match.capturedStart(capture)),
                       int(match.capturedLength(capture)), role});
    }
}

// Hide strings and comments while preserving UTF-16 offsets. This classifier
// is deliberately conservative: lexical styles remain the fallback whenever
// a name cannot be identified safely.
QString maskNonCode(const QString &line, bool hashComments, bool &inBlockComment)
{
    QString masked = line;
    QChar quote;
    bool escaped = false;
    for (int i = 0; i < line.size(); ++i)
    {
        const QChar current = line.at(i);
        const QChar next = i + 1 < line.size() ? line.at(i + 1) : QChar();
        if (inBlockComment)
        {
            masked[i] = QLatin1Char(' ');
            if (current == QLatin1Char('*') && next == QLatin1Char('/'))
            {
                masked[++i] = QLatin1Char(' ');
                inBlockComment = false;
            }
            continue;
        }
        if (!quote.isNull())
        {
            masked[i] = QLatin1Char(' ');
            if (!escaped && current == quote)
                quote = QChar();
            escaped = !escaped && current == QLatin1Char('\\');
            if (current != QLatin1Char('\\'))
                escaped = false;
            continue;
        }
        if (current == QLatin1Char('/') && next == QLatin1Char('*'))
        {
            masked[i] = masked[i + 1] = QLatin1Char(' ');
            ++i;
            inBlockComment = true;
        }
        else if (current == QLatin1Char('/') && next == QLatin1Char('/'))
        {
            for (int j = i; j < masked.size(); ++j)
                masked[j] = QLatin1Char(' ');
            break;
        }
        else if (hashComments && current == QLatin1Char('#'))
        {
            for (int j = i; j < masked.size(); ++j)
                masked[j] = QLatin1Char(' ');
            break;
        }
        else if (current == QLatin1Char('\'') || current == QLatin1Char('"') || current == QLatin1Char('`'))
        {
            quote = current;
            masked[i] = QLatin1Char(' ');
            escaped = false;
        }
    }
    return masked;
}

} // namespace

namespace NotePeek {

SemanticAnalysisResult BasicSemanticTokenProvider::analyze(
    const SemanticDocument &document, const SemanticAnalysisControl &control) const
{
    SemanticAnalysisResult result;
    result.revision = document.revision;
    result.firstLine = document.firstLine;
    QElapsedTimer timer;
    timer.start();
    if (document.text.size() > 2 * 1024 * 1024)
        return result;

    const bool pythonLike = document.languageId.compare(QLatin1String("Python"), Qt::CaseInsensitive) == 0
                            || document.languageId.compare(QLatin1String("Starlark"), Qt::CaseInsensitive) == 0;
    const bool codeLike = pythonLike || document.languageId == QLatin1String("C++")
                          || document.languageId == QLatin1String("C#")
                          || document.languageId == QLatin1String("Java")
                          || document.languageId == QLatin1String("JavaScript")
                          || document.languageId == QLatin1String("Rust")
                          || document.languageId == QLatin1String("QML");
    if (!codeLike)
        return result;

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

    bool inBlockComment = false;
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
        const QString code = maskNonCode(lines.at(lineNumber), pythonLike, inBlockComment);
        const int absoluteLine = document.firstLine + lineNumber;
        appendMatches(result.tokens, absoluteLine, code, propertyPattern, 1, SyntaxRole::Property);
        appendMatches(result.tokens, absoluteLine, code, functionPattern, 1, SyntaxRole::Function, controlWords);
        appendMatches(result.tokens, absoluteLine, code, typePattern, 1, SyntaxRole::Type);
        appendMatches(result.tokens, absoluteLine, code, variablePattern, 1, SyntaxRole::Variable);
        appendMatches(result.tokens, absoluteLine, code, macroPattern, 1, SyntaxRole::Builtin);
        if (pythonLike)
        {
            appendMatches(result.tokens, absoluteLine, code, pythonFunctionPattern, 1, SyntaxRole::Function);
            appendMatches(result.tokens, absoluteLine, code, pythonClassPattern, 1, SyntaxRole::Type);
            appendMatches(result.tokens, absoluteLine, code, decoratorPattern, 1, SyntaxRole::Attribute);
        }
        result.lastLine = absoluteLine;
    }
    result.elapsedNanoseconds = timer.nsecsElapsed();
    return result;
}

} // namespace NotePeek
