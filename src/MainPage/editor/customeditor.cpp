#include "editor/customeditor.h"
#include <QMimeData>
#include <QFrame>
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

CustomEditor::CustomEditor(QWidget *parent)
    : QsciScintilla{parent}
{
    setAcceptDrops(true);
    setFrameShape(QFrame::NoFrame);
}

void CustomEditor::setFilePath(const QString &path)
{
    m_filePath = path;
}

QString CustomEditor::filePath() const
{
    return m_filePath;
}

void CustomEditor::setCodeEditingFeaturesEnabled(bool enabled)
{
    m_codeEditingFeaturesEnabled = enabled;
}

bool CustomEditor::codeEditingFeaturesEnabled() const
{
    return m_codeEditingFeaturesEnabled;
}

void CustomEditor::setBracketAssistanceEnabled(bool enabled)
{
    m_bracketAssistanceEnabled = enabled;
}

bool CustomEditor::bracketAssistanceEnabled() const
{
    return m_bracketAssistanceEnabled;
}

void CustomEditor::setDocumentCompletionEnabled(bool enabled)
{
    m_documentCompletionEnabled = enabled;
}

bool CustomEditor::documentCompletionEnabled() const
{
    return m_documentCompletionEnabled;
}

void CustomEditor::toggleCurrentFold()
{
    recolor();

    int line = 0;
    int index = 0;
    getCursorPosition(&line, &index);
    Q_UNUSED(index);

    const int level = int(SendScintilla(QsciScintillaBase::SCI_GETFOLDLEVEL, line));
    if ((level & QsciScintillaBase::SC_FOLDLEVELHEADERFLAG) == 0)
    {
        line = int(SendScintilla(QsciScintillaBase::SCI_GETFOLDPARENT, line));
    }

    if (line >= 0)
    {
        foldLine(line);
    }
}

void CustomEditor::toggleAllFolds()
{
    foldAll();
}

void CustomEditor::goToMatchingBrace()
{
    if (m_bracketAssistanceEnabled)
        moveToMatchingBrace();
}

void CustomEditor::completeWordFromDocument()
{
    if (m_documentCompletionEnabled)
        autoCompleteFromDocument();
}

void CustomEditor::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
    {
        e->acceptProposedAction();
    }
    else
    {
        QsciScintilla::dragEnterEvent(e);
    }
}

void CustomEditor::dropEvent(QDropEvent *e)
{
    const QMimeData *mimeData = e->mimeData();

    if (mimeData->hasUrls())
    {
        QList<QUrl> urlList = mimeData->urls();
        emit filesDropped(urlList);
        e->acceptProposedAction();
    }
    else
    {
        // Not a file drop; let the base class handle text drops etc.
        QsciScintilla::dropEvent(e);
    }
}

void CustomEditor::keyPressEvent(QKeyEvent *e)
{
    if (!e)
    {
        return;
    }

    // Keep Markdown and plain text input free of automatic code punctuation.
    if (isReadOnly() || !m_codeEditingFeaturesEnabled || !m_bracketAssistanceEnabled)
    {
        QsciScintilla::keyPressEvent(e);
        return;
    }

    const Qt::KeyboardModifiers commandModifiers = Qt::ControlModifier
                                                   | Qt::AltModifier
                                                   | Qt::MetaModifier;
    const QString typedText = e->text();
    if (typedText.size() != 1 || (e->modifiers() & commandModifiers))
    {
        QsciScintilla::keyPressEvent(e);
        return;
    }

    const long selectionStart = SendScintilla(QsciScintillaBase::SCI_GETSELECTIONSTART);
    const long selectionEnd = SendScintilla(QsciScintillaBase::SCI_GETSELECTIONEND);
    const bool hasSelection = selectionStart != selectionEnd;

    if (!hasSelection && isClosingCharacter(typedText))
    {
        const long position = SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS);
        if (SendScintilla(QsciScintillaBase::SCI_GETCHARAT, position) == typedText.at(0).toLatin1())
        {
            SendScintilla(QsciScintillaBase::SCI_GOTOPOS, position + 1);
            return;
        }
    }

    const char closeCharacter = matchingCloseCharacter(typedText);
    if (closeCharacter == '\0')
    {
        QsciScintilla::keyPressEvent(e);
        return;
    }

    beginUndoAction();
    if (hasSelection)
    {
        const QString selected = selectedText();
        const QString replacement = typedText + selected + QLatin1Char(closeCharacter);
        const int selectedByteLength = selected.toUtf8().size();
        replaceSelectedText(replacement);
        SendScintilla(QsciScintillaBase::SCI_SETSEL,
                      selectionStart + 1,
                      selectionStart + 1 + selectedByteLength);
    }
    else
    {
        QsciScintilla::keyPressEvent(e);
        const long position = SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS);
        const char closingText[] = {closeCharacter, '\0'};
        SendScintilla(QsciScintillaBase::SCI_INSERTTEXT, position, closingText);
    }
    endUndoAction();
}
