// NotePeek editor extensions for QScintilla.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#include "Qsci/notepeekeditor.h"

#include <QKeyEvent>

#include <algorithm>

namespace {

// UTF-16 index -> UTF-8 byte offset for one line.  Building the whole table in
// a single pass costs O(line length) and replaces the two QString::toUtf8()
// conversions that would otherwise run for every token on the line.
QVector<int> buildUtf8Offsets(const QString &line)
{
    QVector<int> offsets(line.size() + 1, 0);
    int bytes = 0;
    for (int i = 0; i < line.size(); ++i)
    {
        offsets[i] = bytes;
        const char16_t unit = line.at(i).unicode();
        if (unit < 0x80)
            bytes += 1;
        else if (unit < 0x800 || QChar::isSurrogate(unit))
            // A surrogate pair is two UTF-16 units and four UTF-8 bytes, so
            // each half contributes two.
            bytes += 2;
        else
            bytes += 3;
    }
    offsets[line.size()] = bytes;
    return offsets;
}

char matchingCloseCharacter(const QString &text)
{
    if (text == QLatin1String("("))
        return ')';
    if (text == QLatin1String("["))
        return ']';
    if (text == QLatin1String("{"))
        return '}';
    return '\0';
}

bool isClosingCharacter(const QString &text)
{
    return text == QLatin1String(")")
           || text == QLatin1String("]")
           || text == QLatin1String("}");
}

} // namespace

namespace NotePeek {

Editor::Editor(QWidget *parent)
    : QsciScintilla(parent)
{
    // indicatorDefine() returns -1 when every indicator is already taken; the
    // semantic layer then stays inert rather than writing to indicator -1.
    m_semanticIndicator = indicatorDefine(TextColorIndicator);
    if (m_semanticIndicator >= 0)
        SendScintilla(SCI_INDICSETFLAGS, m_semanticIndicator, 1L); // SC_INDICFLAG_VALUEFORE
    connect(this, &QsciScintilla::textChanged, this, [this]() {
        ++m_documentRevision;
    });
    connect(this, &QsciScintillaBase::SCN_MODIFIED, this,
            [this](int, int modificationType, const char *, int, int linesAdded,
                   int line, int, int, int, int) {
        handleSemanticModification(modificationType, linesAdded, line);
    });
}

void Editor::setCodeEditingFeaturesEnabled(bool enabled)
{
    m_codeEditingFeaturesEnabled = enabled;
}

bool Editor::codeEditingFeaturesEnabled() const
{
    return m_codeEditingFeaturesEnabled;
}

void Editor::setBracketAssistanceEnabled(bool enabled)
{
    m_bracketAssistanceEnabled = enabled;
}

bool Editor::bracketAssistanceEnabled() const
{
    return m_bracketAssistanceEnabled;
}

void Editor::setDocumentCompletionEnabled(bool enabled)
{
    m_documentCompletionEnabled = enabled;
}

bool Editor::documentCompletionEnabled() const
{
    return m_documentCompletionEnabled;
}

void Editor::toggleCurrentFold()
{
    int line = 0;
    int index = 0;
    getCursorPosition(&line, &index);
    Q_UNUSED(index);

    // Fold levels only exist for text the lexer has already styled. Recolouring
    // the whole document here stalled the fold shortcut for as long as a full
    // re-lex took on a large file; styling just the not-yet-styled gap up to
    // the cursor gives the same fold information.
    const long lineEnd = SendScintilla(SCI_GETLINEENDPOSITION, line);
    const long styled = SendScintilla(SCI_GETENDSTYLED);
    if (styled < lineEnd)
        recolor(styled, lineEnd);

    const int level = int(SendScintilla(SCI_GETFOLDLEVEL, line));
    if ((level & SC_FOLDLEVELHEADERFLAG) == 0)
        line = int(SendScintilla(SCI_GETFOLDPARENT, line));

    if (line >= 0)
        foldLine(line);
}

void Editor::toggleAllFolds()
{
    foldAll();
}

void Editor::goToMatchingBrace()
{
    if (m_bracketAssistanceEnabled)
        moveToMatchingBrace();
}

void Editor::completeWordFromDocument()
{
    if (m_documentCompletionEnabled)
        autoCompleteFromDocument();
}

quint64 Editor::documentRevision() const
{
    return m_documentRevision;
}

bool Editor::setSemanticTokens(quint64 revision, const QVector<SemanticToken> &tokens)
{
    if (revision != m_documentRevision)
        return false;
    clearSemanticTokens();
    m_semanticTokens = tokens;
    sortSemanticTokens();
    renderSemanticTokens();
    return true;
}

bool Editor::applySemanticTokenDelta(quint64 revision, int firstLine, int lastLine,
                                     const QVector<SemanticToken> &tokens)
{
    if (revision != m_documentRevision || firstLine < 0 || lastLine < firstLine)
        return false;
    lastLine = qMin(lastLine, lines() - 1);
    if (lastLine < firstLine)
        return false;
    clearSemanticIndicatorLines(firstLine, lastLine);

    // The cache is sorted by line, so the replaced window is one contiguous
    // run: erase it in a single splice instead of removing element by element.
    const auto byLine = [](const SemanticToken &token, int line) { return token.line < line; };
    const auto lineFirst = [](int line, const SemanticToken &token) { return line < token.line; };
    const auto begin = std::lower_bound(m_semanticTokens.begin(), m_semanticTokens.end(),
                                        firstLine, byLine);
    const auto end = std::upper_bound(begin, m_semanticTokens.end(), lastLine, lineFirst);
    const int insertAt = int(begin - m_semanticTokens.begin());
    m_semanticTokens.erase(begin, end);

    QVector<SemanticToken> accepted;
    accepted.reserve(tokens.size());
    for (const SemanticToken &token : tokens)
    {
        if (token.line >= firstLine && token.line <= lastLine)
            accepted.append(token);
    }
    std::stable_sort(accepted.begin(), accepted.end(),
                     [](const SemanticToken &a, const SemanticToken &b) { return a.line < b.line; });
    // Make room once and fill it: inserting one at a time would shift the whole
    // tail per token, which is quadratic in the size of the cache.
    m_semanticTokens.insert(insertAt, accepted.size(), SemanticToken());
    for (int i = 0; i < accepted.size(); ++i)
        m_semanticTokens[insertAt + i] = accepted.at(i);
    renderTokenRange(insertAt, insertAt + accepted.size() - 1);
    return true;
}

void Editor::evictSemanticTokensOutside(int keepFirstLine, int keepLastLine, int maxTokens)
{
    if (maxTokens < 0)
        maxTokens = 0;
    if (m_semanticTokens.size() <= maxTokens)
        return;

    // Drop whole lines, furthest from the retained window first, so the region
    // the user is looking at keeps its colours.
    while (m_semanticTokens.size() > maxTokens && !m_semanticTokens.isEmpty())
    {
        const int frontLine = m_semanticTokens.first().line;
        const int backLine = m_semanticTokens.last().line;
        if (frontLine >= keepFirstLine && backLine <= keepLastLine)
            break;
        const int frontDistance = keepFirstLine - frontLine;
        const int backDistance = backLine - keepLastLine;
        const int victimLine = frontDistance >= backDistance ? frontLine : backLine;
        clearSemanticIndicatorLines(victimLine, victimLine);
        if (victimLine == frontLine)
        {
            int count = 0;
            while (count < m_semanticTokens.size() && m_semanticTokens.at(count).line == victimLine)
                ++count;
            m_semanticTokens.remove(0, count);
        }
        else
        {
            while (!m_semanticTokens.isEmpty() && m_semanticTokens.last().line == victimLine)
                m_semanticTokens.removeLast();
        }
    }
}

void Editor::clearSemanticTokens()
{
    if (m_semanticIndicator < 0)
    {
        m_semanticTokens.clear();
        return;
    }
    SendScintilla(SCI_SETINDICATORCURRENT, m_semanticIndicator);
    SendScintilla(SCI_INDICATORCLEARRANGE, 0, SendScintilla(SCI_GETLENGTH));
    m_semanticTokens.clear();
}

void Editor::setSemanticTokenColors(const SyntaxPalette &colors)
{
    if (m_semanticColors == colors)
        return;
    m_semanticColors = colors;
    renderSemanticTokens();
}

QVector<SemanticToken> Editor::semanticTokens() const
{
    return m_semanticTokens;
}

int Editor::semanticTokenCount() const
{
    return m_semanticTokens.size();
}

void Editor::sortSemanticTokens()
{
    std::stable_sort(m_semanticTokens.begin(), m_semanticTokens.end(),
                     [](const SemanticToken &a, const SemanticToken &b) { return a.line < b.line; });
}

void Editor::renderSemanticTokens()
{
    if (m_semanticIndicator < 0)
        return;
    SendScintilla(SCI_SETINDICATORCURRENT, m_semanticIndicator);
    SendScintilla(SCI_INDICATORCLEARRANGE, 0, SendScintilla(SCI_GETLENGTH));
    renderTokenRange(0, m_semanticTokens.size() - 1);
}

void Editor::renderTokenRange(int fromIndex, int toIndex)
{
    fromIndex = qMax(0, fromIndex);
    toIndex = qMin(toIndex, m_semanticTokens.size() - 1);
    int index = fromIndex;
    while (index <= toIndex)
    {
        const int line = m_semanticTokens.at(index).line;
        int runEnd = index;
        while (runEnd + 1 <= toIndex && m_semanticTokens.at(runEnd + 1).line == line)
            ++runEnd;
        renderSemanticTokensForLine(line, index, runEnd);
        index = runEnd + 1;
    }
}

void Editor::renderSemanticTokensForLine(int line, int fromIndex, int toIndex)
{
    if (m_semanticIndicator < 0 || line < 0 || line >= lines())
        return;
    const QString lineText = text(line);
    const QVector<int> byteOffsets = buildUtf8Offsets(lineText);
    const long lineStart = SendScintilla(SCI_POSITIONFROMLINE, line);
    SendScintilla(SCI_SETINDICATORCURRENT, m_semanticIndicator);
    for (int i = fromIndex; i <= toIndex; ++i)
    {
        const SemanticToken &token = m_semanticTokens.at(i);
        if (token.utf16Column < 0 || token.utf16Length <= 0)
            continue;
        if (token.utf16Column + token.utf16Length > lineText.size())
            continue;
        const QColor color = m_semanticColors.value(token.role);
        if (!color.isValid())
            continue;
        const int startByte = byteOffsets.at(token.utf16Column);
        const int endByte = byteOffsets.at(token.utf16Column + token.utf16Length);
        const long scintillaColor = (color.blue() << 16) | (color.green() << 8) | color.red();
        SendScintilla(SCI_SETINDICATORVALUE, SC_INDICVALUEBIT | scintillaColor);
        SendScintilla(SCI_INDICATORFILLRANGE, lineStart + startByte, long(endByte - startByte));
    }
}

void Editor::clearSemanticIndicatorLines(int firstLine, int lastLine)
{
    if (m_semanticIndicator < 0 || lines() <= 0)
        return;
    firstLine = qBound(0, firstLine, lines() - 1);
    lastLine = qBound(firstLine, lastLine, lines() - 1);
    const long start = SendScintilla(SCI_POSITIONFROMLINE, firstLine);
    const long end = lastLine + 1 < lines()
        ? SendScintilla(SCI_POSITIONFROMLINE, lastLine + 1)
        : SendScintilla(SCI_GETLENGTH);
    SendScintilla(SCI_SETINDICATORCURRENT, m_semanticIndicator);
    SendScintilla(SCI_INDICATORCLEARRANGE, start, qMax(0L, end - start));
}

void Editor::handleSemanticModification(int modificationType, int linesAdded, int line)
{
    if ((modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) == 0)
        return;

    for (int i = m_semanticTokens.size() - 1; i >= 0; --i)
    {
        SemanticToken &token = m_semanticTokens[i];
        if (token.line == line)
        {
            m_semanticTokens.removeAt(i);
            continue;
        }
        if (linesAdded > 0 && token.line > line)
        {
            token.line += linesAdded;
        }
        else if (linesAdded < 0 && token.line > line)
        {
            const int deletedLastLine = line - linesAdded;
            if (token.line <= deletedLastLine)
                m_semanticTokens.removeAt(i);
            else
                token.line += linesAdded;
        }
    }
    clearSemanticIndicatorLines(line, line + qMax(0, linesAdded));
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    if (!event)
        return;

    if (isReadOnly() || !m_codeEditingFeaturesEnabled || !m_bracketAssistanceEnabled)
    {
        QsciScintilla::keyPressEvent(event);
        return;
    }

    const Qt::KeyboardModifiers commandModifiers = Qt::ControlModifier
                                                   | Qt::AltModifier
                                                   | Qt::MetaModifier;
    const QString typedText = event->text();
    if (typedText.size() != 1 || (event->modifiers() & commandModifiers))
    {
        QsciScintilla::keyPressEvent(event);
        return;
    }

    const long selectionStart = SendScintilla(SCI_GETSELECTIONSTART);
    const long selectionEnd = SendScintilla(SCI_GETSELECTIONEND);
    const bool hasSelection = selectionStart != selectionEnd;

    if (!hasSelection && isClosingCharacter(typedText))
    {
        const long position = SendScintilla(SCI_GETCURRENTPOS);
        if (SendScintilla(SCI_GETCHARAT, position) == typedText.at(0).toLatin1())
        {
            SendScintilla(SCI_GOTOPOS, position + 1);
            return;
        }
    }

    const char closeCharacter = matchingCloseCharacter(typedText);
    if (closeCharacter == '\0')
    {
        QsciScintilla::keyPressEvent(event);
        return;
    }

    beginUndoAction();
    if (hasSelection)
    {
        const QString selected = selectedText();
        const QString replacement = typedText + selected + QLatin1Char(closeCharacter);
        const int selectedByteLength = selected.toUtf8().size();
        replaceSelectedText(replacement);
        SendScintilla(SCI_SETSEL,
                      selectionStart + 1,
                      selectionStart + 1 + selectedByteLength);
    }
    else
    {
        QsciScintilla::keyPressEvent(event);
        const long position = SendScintilla(SCI_GETCURRENTPOS);
        const char closingText[] = {closeCharacter, '\0'};
        SendScintilla(SCI_INSERTTEXT, position, closingText);
    }
    endUndoAction();
}

} // namespace NotePeek
