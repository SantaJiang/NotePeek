#include "findreplacemanager.h"
#include "../editor/multieditor.h"
#include "../editor/customeditor.h"
#include "../decorators/highlightedscrollbardecorator.h"
#include <QMessageBox>
#include <QFileInfo>
#include <QTimer>
#include <QScrollBar>

FindReplaceManager::FindReplaceManager(QObject *parent)
    : QObject(parent)
    , m_editor(nullptr)
    , m_multiEditor(nullptr)
    , m_caseSensitive(false)
    , m_wholeWord(false)
    , m_wrapAround(true)
    , m_backwards(false)
    , m_highlightIndicator(8)
    , m_savedFirstVisibleLine(0)
{
}

FindReplaceManager::~FindReplaceManager()
{
}

void FindReplaceManager::setCurrentEditor(CustomEditor *editor)
{
    m_editor = editor;
    initializeEditor();
}

void FindReplaceManager::setMultiEditor(MultiEditor *multiEditor)
{
    m_multiEditor = multiEditor;
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
        m_initialFindText = selectedText;
        highlightAllMatches();
    }

    addToFindHistory(m_findText);

    bool re = false;
    bool cs = m_caseSensitive;
    bool wo = m_wholeWord;
    bool wrap = m_wrapAround;
    bool forward = !m_backwards;

    m_editor->findFirst(m_findText, re, cs, wo, wrap, forward);
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

        bool re = false;
        bool cs = m_caseSensitive;
        bool wo = m_wholeWord;
        bool wrap = false;

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

    bool re = false;
    bool cs = m_caseSensitive;
    bool wo = m_wholeWord;

    m_editor->setCursorPosition(0, 0);

    int replaceCount = 0;
    bool found = m_editor->findFirst(m_findText, re, cs, wo, false, true);

    while (found) {
        m_editor->replaceSelectedText(m_replaceText);
        replaceCount++;
        found = m_editor->findNext();
    }

    m_editor->setCursorPosition(currentLine, currentIndex);

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
    bool hadSelection = m_editor->hasSelectedText();
    int selLineFrom, selIndexFrom, selLineTo, selIndexTo;
    if (hadSelection) {
        m_editor->getSelection(&selLineFrom, &selIndexFrom, &selLineTo, &selIndexTo);
    }

    bool re = false;
    bool cs = m_caseSensitive;
    bool wo = m_wholeWord;
    bool wrap = false;
    bool forward = true;

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

    if (hadSelection) {
        m_editor->setSelection(selLineFrom, selIndexFrom, selLineTo, selIndexTo);
    } else {
        m_editor->setCursorPosition(originalLine, originalIndex);
    }

    if (m_initialFindText.isEmpty() && !m_findText.isEmpty()) {
        selectFirstMatch();
    }

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
    QTimer::singleShot(0, this, [this]() {
        clearHighlights();
        highlightAllMatches();
        emit highlightsRefreshed();
    });
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
    // 历史记录管理实现
    Q_UNUSED(text)
}

void FindReplaceManager::addToReplaceHistory(const QString &text)
{
    // 历史记录管理实现
    Q_UNUSED(text)
}

void FindReplaceManager::selectFirstMatch()
{
    if (!m_editor || m_findText.isEmpty())
        return;

    bool re = false;
    bool cs = m_caseSensitive;
    bool wo = m_wholeWord;
    bool wrap = m_wrapAround;
    bool forward = !m_backwards;

    m_editor->findFirst(m_findText, re, cs, wo, wrap, forward);
}

void FindReplaceManager::updateScrollBarMarkers()
{
    if (!m_editor)
        return;

    QVariant decoratorVar = m_editor->property("scrollDecorator");
    if (decoratorVar.isValid()) {
        HighlightedScrollBarDecorator *decorator = decoratorVar.value<HighlightedScrollBarDecorator*>();
        if (decorator) {
            decorator->notify(nullptr);
        }
    } else {
        m_editor->verticalScrollBar()->update();
    }
}

void FindReplaceManager::initializeEditor()
{
    if (!m_editor)
        return;

    m_editor->indicatorDefine(CustomEditor::FullBoxIndicator, m_highlightIndicator);
    QColor highlightColor("#F59E0B");
    long color = (highlightColor.blue() << 16) | (highlightColor.green() << 8) | highlightColor.red();
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETFORE, m_highlightIndicator, color);
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETUNDER, m_highlightIndicator, true);
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETOUTLINEALPHA, (unsigned long)m_highlightIndicator, (long)0);
}

bool FindReplaceManager::isLargeFile() const
{
    if (!m_editor)
        return false;

    QString filePath = m_editor->property("filePath").toString();
    QFileInfo fileInfo(filePath);
    return (fileInfo.size() > 10 * 1024 * 1024);
}
