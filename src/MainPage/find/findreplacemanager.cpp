#include "find/findreplacemanager.h"
#include "themecolors.h"
#include "editor/customeditor.h"
#include "foundation/performanceconstants.h"
#include "decorators/highlightedscrollbardecorator.h"
#include <QMessageBox>
#include <QTimer>
#include <QScrollBar>

FindReplaceManager::FindReplaceManager(QObject *parent)
    : QObject(parent)
    , m_editor(nullptr)
    , m_caseSensitive(false)
    , m_wholeWord(false)
    , m_wrapAround(true)
    , m_backwards(false)
    , m_highlightIndicator(-1)
    , m_highlightTimer(new QTimer(this))
{
    m_highlightTimer->setSingleShot(true);
    m_highlightTimer->setInterval(180);
    connect(m_highlightTimer, &QTimer::timeout, this, [this]() {
        if (!m_editor)
            return;
        highlightAllMatches();
        emit highlightsRefreshed();
    });
}

void FindReplaceManager::setCurrentEditor(CustomEditor *editor)
{
    if (m_editor == editor)
        return;

    clearHighlights();
    m_highlightTimer->stop();
    m_editor = editor;
    initializeEditor();
    if (!m_findText.isEmpty())
    {
        refreshHighlights();
    }
}

void FindReplaceManager::find(const QString &text)
{
    if (!text.isEmpty()) {
        setFindText(text);
    }

    if (!m_editor) {
        QMessageBox::information(nullptr, tr("Find"), tr("No active editor."));
        return;
    }

    QString selectedText = m_editor->selectedText();
    if (!selectedText.isEmpty() && m_findText.isEmpty()) {
        setFindText(selectedText);
        highlightAllMatches();
    }

    if (m_findText.isEmpty()) {
        clearHighlights();
        return;
    }

    addToFindHistory(m_findText);

    bool re, cs, wo, wrap, forward;
    prepareFindOptions(re, cs, wo, wrap, forward, m_wrapAround);

    int originalLine, originalIndex;
    m_editor->getCursorPosition(&originalLine, &originalIndex);
    const int firstVisibleLine = m_editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
    const int xOffset = m_editor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET);
    const bool found = m_editor->findFirst(m_findText, re, cs, wo, wrap, forward);
    if (!found)
    {
        m_editor->setCursorPosition(originalLine, originalIndex);
        m_editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
        m_editor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);
    }
    emit findRequested();
}

void FindReplaceManager::findNext()
{
    if (!m_editor) {
        QMessageBox::information(nullptr, tr("Find"), tr("No active editor."));
        return;
    }

    if (m_findText.isEmpty()) {
        find();
        return;
    }

    bool found = m_editor->findNext();

    if (!found && m_wrapAround) {
        bool forward = !m_backwards;
        int line = forward ? 0 : m_editor->lines() - 1;
        int index = forward ? 0 : m_editor->lineLength(line);

        bool re, cs, wo, wrap;
        prepareFindOptions(re, cs, wo, wrap, forward, false);

        m_editor->findFirst(m_findText, re, cs, wo, wrap, forward, line, index);
    }
}

void FindReplaceManager::replace(const QString &replaceText)
{
    if (!replaceText.isEmpty()) {
        setReplaceText(replaceText);
    }

    if (!m_editor) {
        QMessageBox::information(nullptr, tr("Replace"), tr("No active editor."));
        return;
    }

    if (m_findText.isEmpty()) {
        const QString selectedText = m_editor->selectedText();
        if (!selectedText.isEmpty()) {
            setFindText(selectedText);
        } else {
            QMessageBox::information(nullptr, tr("Replace"), tr("The search field is empty."));
            return;
        }
    }

    addToReplaceHistory(m_replaceText);

    bool selectedMatch = false;
    if (m_editor->hasSelectedText())
    {
        const Qt::CaseSensitivity sensitivity = m_caseSensitive
                                                    ? Qt::CaseSensitive
                                                    : Qt::CaseInsensitive;
        selectedMatch = m_editor->selectedText().compare(m_findText, sensitivity) == 0;
        if (selectedMatch && m_wholeWord)
        {
            const long selectionStart = m_editor->SendScintilla(
                QsciScintillaBase::SCI_GETSELECTIONSTART);
            const long selectionEnd = m_editor->SendScintilla(
                QsciScintillaBase::SCI_GETSELECTIONEND);
            selectedMatch = m_editor->SendScintilla(
                QsciScintillaBase::SCI_ISRANGEWORD, selectionStart, selectionEnd) != 0;
        }
    }

    // The Replace command is defined as replacing the current search match.
    // A user-created unrelated selection must never be overwritten; move to
    // the next real match first and let a subsequent Replace act on it.
    if (!selectedMatch)
    {
        find();
        return;
    }

    m_editor->replaceSelectedText(m_replaceText);

    findNext();
    refreshHighlights();
    emit replaceRequested();
}

void FindReplaceManager::replaceAll()
{
    if (!m_editor) {
        QMessageBox::information(nullptr, tr("Replace All"), tr("No active editor."));
        return;
    }

    if (m_findText.isEmpty()) {
        QMessageBox::information(nullptr, tr("Replace All"), tr("The search field is empty."));
        return;
    }

    addToFindHistory(m_findText);
    addToReplaceHistory(m_replaceText);

    int currentLine, currentIndex;
    m_editor->getCursorPosition(&currentLine, &currentIndex);
    const int firstVisibleLine = m_editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);

    bool re, cs, wo, wrap, forward;
    prepareFindOptions(re, cs, wo, wrap, forward, false);
    forward = true; // Always forward for replaceAll

    m_editor->setCursorPosition(0, 0);

    int replaceCount = 0;
    bool found = m_editor->findFirst(m_findText, re, cs, wo, wrap, forward);

    // One undo step for the whole pass, and no repaints per match — a large
    // replace-all otherwise restyles and repaints the view once per occurrence.
    m_editor->setUpdatesEnabled(false);
    m_editor->beginUndoAction();
    while (found) {
        m_editor->replaceSelectedText(m_replaceText);
        replaceCount++;
        found = m_editor->findNext();
    }
    m_editor->endUndoAction();
    m_editor->setUpdatesEnabled(true);

    m_editor->setCursorPosition(currentLine, currentIndex);
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);

    QMessageBox::information(nullptr, tr("Replace All"),
                             tr("Replaced %1 occurrence(s).").arg(replaceCount));

    refreshHighlights();
    emit replaceAllRequested();
}

void FindReplaceManager::highlightAllMatches()
{
    if (!m_editor)
        return;
    if (isLargeFile())
    {
        // A document can cross the large-file threshold after highlights were
        // already present. Clear the old indicator layer once instead of
        // leaving stale matches visible indefinitely.
        if (!m_searchResults.isEmpty())
            clearHighlights();
        return;
    }

    initializeEditor();

    if (m_findText.isEmpty()) {
        clearHighlights();
        return;
    }

    clearHighlights();

    int originalLine = 0;
    int originalIndex = 0;
    m_editor->getCursorPosition(&originalLine, &originalIndex);
    const int firstVisibleLine = int(m_editor->SendScintilla(
        QsciScintillaBase::SCI_GETFIRSTVISIBLELINE));
    const int xOffset = int(m_editor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET));
    const bool hadSelection = m_editor->hasSelectedText();
    int selectionFromLine = 0;
    int selectionFromIndex = 0;
    int selectionToLine = 0;
    int selectionToIndex = 0;
    int selectionPosition = -1;
    int selectionLength = 0;
    if (hadSelection)
    {
        m_editor->getSelection(&selectionFromLine, &selectionFromIndex,
                               &selectionToLine, &selectionToIndex);
        selectionPosition = m_editor->positionFromLineIndex(selectionFromLine,
                                                            selectionFromIndex);
        selectionLength = m_editor->positionFromLineIndex(selectionToLine,
                                                          selectionToIndex)
                          - selectionPosition;
    }
    bool selectedMatchFound = false;

    m_searchResults.clear();
    const QByteArray needle = m_findText.toUtf8();
    const long documentLength = m_editor->SendScintilla(QsciScintillaBase::SCI_GETLENGTH);
    int searchFlags = 0;
    if (m_caseSensitive)
        searchFlags |= QsciScintillaBase::SCFIND_MATCHCASE;
    if (m_wholeWord)
        searchFlags |= QsciScintillaBase::SCFIND_WHOLEWORD;
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETSEARCHFLAGS, searchFlags);
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT,
                            m_highlightIndicator);

    long searchFrom = 0;
    while (searchFrom < documentLength)
    {
        m_editor->SendScintilla(QsciScintillaBase::SCI_SETTARGETSTART, searchFrom);
        m_editor->SendScintilla(QsciScintillaBase::SCI_SETTARGETEND, documentLength);
        const long position = m_editor->SendScintilla(
            QsciScintillaBase::SCI_SEARCHINTARGET, needle.size(), needle.constData());
        if (position < 0)
            break;
        const long matchEnd = m_editor->SendScintilla(QsciScintillaBase::SCI_GETTARGETEND);
        const long matchLength = qMax(0L, matchEnd - position);
        if (matchLength <= 0)
            break;

        m_editor->SendScintilla(QsciScintillaBase::SCI_INDICATORFILLRANGE,
                                position, matchLength);
        SearchResult result;
        result.position = int(position);
        result.length = int(matchLength);
        result.text = m_findText;
        m_searchResults.append(result);
        selectedMatchFound = selectedMatchFound
                             || (result.position == selectionPosition
                                 && result.length == selectionLength);
        searchFrom = matchEnd;
    }

    if (hadSelection && selectedMatchFound)
    {
        m_editor->setSelection(selectionFromLine, selectionFromIndex,
                               selectionToLine, selectionToIndex);
    }
    else
    {
        m_editor->setCursorPosition(originalLine, originalIndex);
    }
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE,
                            firstVisibleLine);
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);

    updateScrollBarMarkers();
    emit searchResultsChanged();
}

void FindReplaceManager::clearHighlights()
{
    if (!m_editor)
        return;

    m_editor->clearIndicatorRange(0, 0, m_editor->lines(), m_editor->lineLength(m_editor->lines() - 1), m_highlightIndicator);
    m_searchResults.clear();
    updateScrollBarMarkers();
    emit searchResultsChanged();
}

void FindReplaceManager::refreshHighlights()
{
    if (m_editor)
        m_highlightTimer->start();
}

void FindReplaceManager::setFindText(const QString &text)
{
    if (m_findText != text) {
        m_findText = text;
        refreshHighlights();
    }
}

void FindReplaceManager::setReplaceText(const QString &text)
{
    if (m_replaceText != text) {
        m_replaceText = text;
    }
}

void FindReplaceManager::setCaseSensitive(bool on)
{
    if (m_caseSensitive != on) {
        m_caseSensitive = on;
        refreshHighlights();
    }
}

void FindReplaceManager::setWholeWord(bool on)
{
    if (m_wholeWord != on) {
        m_wholeWord = on;
        refreshHighlights();
    }
}

void FindReplaceManager::setWrapAround(bool on)
{
    if (m_wrapAround != on) {
        m_wrapAround = on;
    }
}

void FindReplaceManager::setBackwards(bool backwards)
{
    if (m_backwards != backwards) {
        m_backwards = backwards;
    }
}

void FindReplaceManager::addToFindHistory(const QString &text)
{
    // History tracking not implemented yet
    Q_UNUSED(text);
}

void FindReplaceManager::addToReplaceHistory(const QString &text)
{
    // History tracking not implemented yet
    Q_UNUSED(text);
}

void FindReplaceManager::updateScrollBarMarkers()
{
    if (!m_editor)
        return;

    HighlightedScrollBarDecorator *decorator = m_editor->findChild<HighlightedScrollBarDecorator*>();
    if (decorator)
        decorator->notify(nullptr);
    else
        m_editor->verticalScrollBar()->update();
}

void FindReplaceManager::initializeEditor()
{
    if (!m_editor)
        return;

    // Reuse the same indicator number as the scrollbar decorator so that
    // search-result ticks appear in the custom scrollbar.
    HighlightedScrollBarDecorator *decorator = m_editor->findChild<HighlightedScrollBarDecorator*>();
    if (decorator)
        m_highlightIndicator = decorator->getSearchIndicator();
    if (m_highlightIndicator < 0)
        m_highlightIndicator = m_editor->indicatorDefine(CustomEditor::FullBoxIndicator);

    m_editor->indicatorDefine(CustomEditor::FullBoxIndicator, m_highlightIndicator);
    QColor highlightColor = ThemeColors::FindReplace::highlightIndicator();
    long color = (highlightColor.blue() << 16) | (highlightColor.green() << 8) | highlightColor.red();
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETFORE, m_highlightIndicator, color);
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETUNDER, m_highlightIndicator, true);
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETOUTLINEALPHA, (unsigned long)m_highlightIndicator, (long)0);
}

bool FindReplaceManager::isLargeFile() const
{
    if (!m_editor)
        return false;

    // The in-memory buffer is the authority: an unsaved buffer has no on-disk
    // size, and the disk size can differ from what is actually being searched.
    return m_editor->length() > PerformanceConstants::LargeFileSize;
}

void FindReplaceManager::prepareFindOptions(bool &re, bool &cs, bool &wo, bool &wrap, bool &forward, bool wrapAround) const
{
    re = false; // regex not supported yet
    cs = m_caseSensitive;
    wo = m_wholeWord;
    wrap = wrapAround ? m_wrapAround : false;
    forward = !m_backwards;
}
