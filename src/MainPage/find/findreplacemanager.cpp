#include "find/findreplacemanager.h"
#include "themecolors.h"
#include "editor/customeditor.h"
#include "editor/performanceconstants.h"
#include "decorators/highlightedscrollbardecorator.h"
#include <QMessageBox>
#include <QFileInfo>
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

    if (m_editor->hasSelectedText()) {
        m_editor->replaceSelectedText(m_replaceText);
    }

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

    while (found) {
        m_editor->replaceSelectedText(m_replaceText);
        replaceCount++;
        found = m_editor->findNext();
    }

    m_editor->setCursorPosition(currentLine, currentIndex);
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);

    QMessageBox::information(nullptr, tr("Replace All"),
                             tr("Replaced %1 occurrence(s).").arg(replaceCount));

    refreshHighlights();
    emit replaceAllRequested();
}

void FindReplaceManager::highlightAllMatches()
{
    if (!m_editor || isLargeFile())
        return;

    initializeEditor();

    if (m_findText.isEmpty()) {
        clearHighlights();
        return;
    }

    clearHighlights();

    int originalLine, originalIndex;
    m_editor->getCursorPosition(&originalLine, &originalIndex);
    const int firstVisibleLine = m_editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
    const int xOffset = m_editor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET);
    bool hadSelection = m_editor->hasSelectedText();
    int selLineFrom = 0;
    int selIndexFrom = 0;
    int selLineTo = 0;
    int selIndexTo = 0;
    int selectionPosition = -1;
    int selectionLength = 0;
    if (hadSelection) {
        m_editor->getSelection(&selLineFrom, &selIndexFrom, &selLineTo, &selIndexTo);
        selectionPosition = m_editor->positionFromLineIndex(selLineFrom, selIndexFrom);
        selectionLength = m_editor->positionFromLineIndex(selLineTo, selIndexTo) - selectionPosition;
    }

    bool re, cs, wo, wrap, forward;
    prepareFindOptions(re, cs, wo, wrap, forward, false);
    forward = true; // Always forward for highlightAllMatches

    m_editor->setCursorPosition(0, 0);
    bool found = m_editor->findFirst(m_findText, re, cs, wo, wrap, forward);

    m_searchResults.clear();

    while (found) {
        int lineFrom, indexFrom, lineTo, indexTo;
        m_editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);

        m_editor->fillIndicatorRange(lineFrom, indexFrom, lineTo, indexTo, m_highlightIndicator);

        SearchResult result;
        result.position = m_editor->positionFromLineIndex(lineFrom, indexFrom);
        result.length = (lineFrom == lineTo) ? (indexTo - indexFrom) :
                            (m_editor->positionFromLineIndex(lineTo, indexTo) - result.position);
        result.text = m_findText;
        m_searchResults.append(result);

        found = m_editor->findNext();
    }

    if (hadSelection && selectionMatchesSearchResults(selectionPosition, selectionLength)) {
        m_editor->setSelection(selLineFrom, selIndexFrom, selLineTo, selIndexTo);
    } else {
        m_editor->setCursorPosition(originalLine, originalIndex);
    }
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
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

    QString filePath = m_editor->filePath();
    QFileInfo fileInfo(filePath);
    return (fileInfo.size() > PerformanceConstants::LargeFileSize);
}

bool FindReplaceManager::selectionMatchesSearchResults(int position, int length) const
{
    for (const SearchResult &result : m_searchResults)
    {
        if (result.position == position && result.length == length)
        {
            return true;
        }
    }

    return false;
}

void FindReplaceManager::prepareFindOptions(bool &re, bool &cs, bool &wo, bool &wrap, bool &forward, bool wrapAround) const
{
    re = false; // regex not supported yet
    cs = m_caseSensitive;
    wo = m_wholeWord;
    wrap = wrapAround ? m_wrapAround : false;
    forward = !m_backwards;
}
