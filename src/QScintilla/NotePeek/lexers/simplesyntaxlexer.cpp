#include "NotePeek/lexers/simplesyntaxlexer.h"

#include <Qsci/qsciscintillabase.h>
#include <Qsci/qsciscintilla.h>

#include <algorithm>

namespace {

QSet<QByteArray> makeWordSet(const char *words, bool caseInsensitive)
{
    QSet<QByteArray> set;
    const QList<QByteArray> parts = QByteArray(words ? words : "").split(' ');
    for (QByteArray word : parts)
    {
        word = word.trimmed();
        if (word.isEmpty())
            continue;

        set.insert(caseInsensitive ? word.toLower() : word);
    }
    return set;
}

bool isWordStart(char ch)
{
    return (ch >= 'A' && ch <= 'Z')
           || (ch >= 'a' && ch <= 'z')
           || ch == '_';
}

bool isWordChar(char ch)
{
    return isWordStart(ch)
           || (ch >= '0' && ch <= '9')
           || ch == '-'
           || ch == '.';
}

bool isSpace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

int bracketedLength(const QByteArray &text, int index, char close)
{
    int i = index;
    while (i < text.size())
    {
        if (text.at(i) == close)
            return i - index + 1;
        ++i;
    }
    return text.size() - index;
}

} // namespace

SimpleSyntaxLexer::SimpleSyntaxLexer(const char *languageName,
                                     const char *lineComment,
                                     const char *keywords,
                                     const char *builtins,
                                     const char *directives,
                                     const char *operators,
                                     bool caseInsensitive,
                                     VariableMode variableMode,
                                     bool doubleDashFlags,
                                     QObject *parent)
    : QsciLexerCustom(parent)
    , m_languageName(languageName ? languageName : "")
    , m_lineComment(lineComment ? lineComment : "")
    , m_operators(operators ? operators : "")
    , m_keywords(makeWordSet(keywords, caseInsensitive))
    , m_builtins(makeWordSet(builtins, caseInsensitive))
    , m_directives(makeWordSet(directives, caseInsensitive))
    , m_caseInsensitive(caseInsensitive)
    , m_variableMode(variableMode)
    , m_doubleDashFlags(doubleDashFlags)
{
}

const char *SimpleSyntaxLexer::language() const
{
    return m_languageName.constData();
}

const char *SimpleSyntaxLexer::wordCharacters() const
{
    return "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.";
}

QString SimpleSyntaxLexer::description(int style) const
{
    switch (style)
    {
    case Default:
        return QStringLiteral("Default");
    case Comment:
        return QStringLiteral("Comment");
    case Number:
        return QStringLiteral("Number");
    case Keyword:
        return QStringLiteral("Keyword");
    case DoubleQuotedString:
        return QStringLiteral("Double-quoted string");
    case SingleQuotedString:
        return QStringLiteral("Single-quoted string");
    case Directive:
        return QStringLiteral("Directive");
    case Operator:
        return QStringLiteral("Operator");
    case Identifier:
        return QStringLiteral("Identifier");
    case Builtin:
        return QStringLiteral("Builtin");
    default:
        return QString();
    }
}

void SimpleSyntaxLexer::styleText(int start, int end)
{
    if (!editor() || end <= start)
        return;

    const int lineCount = editor()->SendScintilla(QsciScintillaBase::SCI_GETLINECOUNT);
    const int documentLength = editor()->SendScintilla(QsciScintillaBase::SCI_GETLENGTH);
    const int startLine = editor()->SendScintilla(QsciScintillaBase::SCI_LINEFROMPOSITION, start);
    const int endLine = editor()->SendScintilla(QsciScintillaBase::SCI_LINEFROMPOSITION, end);

    startStyling(start);

    for (int line = startLine; line <= endLine && line < lineCount; ++line)
    {
        const int lineStart = editor()->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, line);
        const int nextLineStart = (line + 1 < lineCount)
                                      ? editor()->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, line + 1)
                                      : documentLength;
        const int segmentStart = std::max(lineStart, start);
        const int segmentEnd = std::min(nextLineStart, end);
        if (segmentEnd <= segmentStart)
            continue;

        const QByteArray lineBytes = editor()->bytes(segmentStart, segmentEnd - 1);
        int i = 0;
        while (i < lineBytes.size())
        {
            if (!m_lineComment.isEmpty() && lineBytes.mid(i, m_lineComment.size()) == m_lineComment)
            {
                setStyling(lineBytes.size() - i, Comment);
                break;
            }

            const int variableLen = variableLength(lineBytes, i);
            if (variableLen > 0)
            {
                setStyling(variableLen, Builtin);
                i += variableLen;
                continue;
            }

            const int flagLen = flagLength(lineBytes, i);
            if (flagLen > 0)
            {
                setStyling(flagLen, Directive);
                i += flagLen;
                continue;
            }

            const char ch = lineBytes.at(i);
            if (ch == '"')
            {
                const int len = quotedStringLength(lineBytes, i, '"');
                setStyling(len, DoubleQuotedString);
                i += len;
                continue;
            }

            if (ch == '\'')
            {
                const int len = quotedStringLength(lineBytes, i, '\'');
                setStyling(len, SingleQuotedString);
                i += len;
                continue;
            }

            const int numberLen = numberLength(lineBytes, i);
            if (numberLen > 0)
            {
                setStyling(numberLen, Number);
                i += numberLen;
                continue;
            }

            const int tokenLen = wordLength(lineBytes, i);
            if (tokenLen > 0)
            {
                const QByteArray word = lineBytes.mid(i, tokenLen);
                setStyling(tokenLen, styleForWord(word));
                i += tokenLen;
                continue;
            }

            if (m_operators.contains(ch))
                setStyling(1, Operator);
            else
                setStyling(1, Default);
            ++i;
        }
    }
}

int SimpleSyntaxLexer::variableLength(const QByteArray &text, int index) const
{
    if (m_variableMode == ShellVariables)
    {
        if (text.at(index) != '$')
            return 0;

        if (index + 1 < text.size() && text.at(index + 1) == '{')
            return 2 + bracketedLength(text, index + 2, '}');

        int i = index + 1;
        while (i < text.size() && (isWordChar(text.at(i)) || text.at(i) == '_'))
            ++i;

        return i > index + 1 ? i - index : 1;
    }

    if (m_variableMode == QMakeVariables)
    {
        if (index + 1 >= text.size() || text.at(index) != '$' || text.at(index + 1) != '$')
            return 0;

        if (index + 2 < text.size())
        {
            const char next = text.at(index + 2);
            if (next == '{')
                return 3 + bracketedLength(text, index + 3, '}');
            if (next == '[')
                return 3 + bracketedLength(text, index + 3, ']');
            if (next == '(')
                return 3 + bracketedLength(text, index + 3, ')');
        }

        int i = index + 2;
        while (i < text.size() && (isWordChar(text.at(i)) || text.at(i) == '_'))
            ++i;

        return i > index + 2 ? i - index : 2;
    }

    return 0;
}

int SimpleSyntaxLexer::flagLength(const QByteArray &text, int index) const
{
    if (!m_doubleDashFlags || index + 1 >= text.size())
        return 0;

    if (text.at(index) != '-' || text.at(index + 1) != '-')
        return 0;

    int i = index + 2;
    while (i < text.size() && !isSpace(text.at(i)))
        ++i;
    return i - index;
}

int SimpleSyntaxLexer::quotedStringLength(const QByteArray &text, int index, char quote) const
{
    int i = index + 1;
    bool escaped = false;
    while (i < text.size())
    {
        const char ch = text.at(i);
        if (escaped)
        {
            escaped = false;
        }
        else if (ch == '\\')
        {
            escaped = true;
        }
        else if (ch == quote)
        {
            return i - index + 1;
        }
        ++i;
    }
    return text.size() - index;
}

int SimpleSyntaxLexer::numberLength(const QByteArray &text, int index) const
{
    const char ch = text.at(index);
    if (!(ch >= '0' && ch <= '9'))
        return 0;

    int i = index + 1;
    while (i < text.size())
    {
        const char current = text.at(i);
        if ((current >= '0' && current <= '9')
            || (current >= 'A' && current <= 'F')
            || (current >= 'a' && current <= 'f')
            || current == 'x'
            || current == 'X'
            || current == '.'
            || current == '_')
        {
            ++i;
            continue;
        }
        break;
    }
    return i - index;
}

int SimpleSyntaxLexer::wordLength(const QByteArray &text, int index) const
{
    if (!isWordStart(text.at(index)))
        return 0;

    int i = index + 1;
    while (i < text.size() && isWordChar(text.at(i)))
        ++i;
    return i - index;
}

int SimpleSyntaxLexer::styleForWord(const QByteArray &word) const
{
    const QByteArray normalized = normalizedWord(word);
    if (m_keywords.contains(normalized))
        return Keyword;
    if (m_builtins.contains(normalized))
        return Builtin;
    if (m_directives.contains(normalized))
        return Directive;
    return Identifier;
}

QByteArray SimpleSyntaxLexer::normalizedWord(const QByteArray &word) const
{
    return m_caseInsensitive ? word.toLower() : word;
}
