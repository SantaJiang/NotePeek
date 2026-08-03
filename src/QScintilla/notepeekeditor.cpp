// NotePeek editor extensions for QScintilla.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#include "Qsci/notepeekeditor.h"

#include <QKeyEvent>

namespace {

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
    indicatorDefine(TextColorIndicator, m_semanticIndicator);
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
    recolor();

    int line = 0;
    int index = 0;
    getCursorPosition(&line, &index);
    Q_UNUSED(index);

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
    renderSemanticTokens();
    return true;
}

bool Editor::applySemanticTokenDelta(quint64 revision, int firstLine, int lastLine,
                                     const QVector<SemanticToken> &tokens)
{
    if (revision != m_documentRevision || firstLine < 0 || lastLine < firstLine)
        return false;
    lastLine = qMin(lastLine, lines() - 1);
    clearSemanticIndicatorLines(firstLine, lastLine);
    for (int i = m_semanticTokens.size() - 1; i >= 0; --i)
    {
        if (m_semanticTokens.at(i).line >= firstLine && m_semanticTokens.at(i).line <= lastLine)
            m_semanticTokens.removeAt(i);
    }
    for (const SemanticToken &token : tokens)
    {
        if (token.line < firstLine || token.line > lastLine)
            continue;
        m_semanticTokens.append(token);
        renderSemanticToken(token);
    }
    return true;
}

void Editor::clearSemanticTokens()
{
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

void Editor::renderSemanticTokens()
{
    SendScintilla(SCI_SETINDICATORCURRENT, m_semanticIndicator);
    SendScintilla(SCI_INDICATORCLEARRANGE, 0, SendScintilla(SCI_GETLENGTH));
    for (const SemanticToken &token : m_semanticTokens)
        renderSemanticToken(token);
}

void Editor::renderSemanticToken(const SemanticToken &token)
{
    if (token.line < 0 || token.line >= lines() || token.utf16Column < 0 || token.utf16Length <= 0)
        return;
    const QString lineText = text(token.line);
    if (token.utf16Column + token.utf16Length > lineText.size())
        return;
    const QColor color = m_semanticColors.value(token.role);
    if (!color.isValid())
        return;
    const long lineStart = SendScintilla(SCI_POSITIONFROMLINE, token.line);
    const long byteStart = lineStart + lineText.left(token.utf16Column).toUtf8().size();
    const long byteLength = lineText.mid(token.utf16Column, token.utf16Length).toUtf8().size();
    const long scintillaColor = (color.blue() << 16) | (color.green() << 8) | color.red();
    SendScintilla(SCI_SETINDICATORCURRENT, m_semanticIndicator);
    SendScintilla(SCI_SETINDICATORVALUE, SC_INDICVALUEBIT | scintillaColor);
    SendScintilla(SCI_INDICATORFILLRANGE, byteStart, byteLength);
}

void Editor::clearSemanticIndicatorLines(int firstLine, int lastLine)
{
    if (lines() <= 0)
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
