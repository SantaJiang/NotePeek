/*
 * This file is part of NotePeek.
 * Copyright 2025 Santa
 *
 * NotePeek is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * NotePeek is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NotePeek.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "widgets/qscieditorinfostatusbar.h"
#include "editor/customeditor.h"
#include "editor/documentformat.h"
#include "themecolors.h"
#include <QMainWindow>
#include <QTimer>
#include <Qsci/qsciscintilla.h>
#include <Qsci/qsciscintillabase.h>
#include <Qsci/qscilexer.h>

namespace {

// Kept equal on both ends so the row is not visually heavier on one side.
constexpr int kEdgePadding = 12;

} // namespace

QsciEditorInfoStatusBar::QsciEditorInfoStatusBar(QMainWindow *window)
    : QStatusBar(window), currentEditor(nullptr)
{
    setAttribute(Qt::WA_StyledBackground, true);

    auto *leftPadding = new QWidget(this);
    leftPadding->setFixedWidth(kEdgePadding);
    addWidget(leftPadding, 0);

    docType = new StatusLabel(120);
    addWidget(docType, 1);

    docPos = new StatusLabel(270);
    docPos->setContentsMargins(6, 0, 6, 0);
    addPermanentWidget(docPos, 0);

    docSize = new StatusLabel(215);
    docSize->setContentsMargins(6, 0, 6, 0);
    addPermanentWidget(docSize, 0);

    eolFormat = new StatusLabel(165);
    eolFormat->setContentsMargins(6, 0, 6, 0);
    addPermanentWidget(eolFormat, 0);
    eolFormat->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(eolFormat, &StatusLabel::customContextMenuRequested, this, [=](const QPoint &pos)
    {
        emit customContextMenuRequestedForEOLLabel(eolFormat->mapToGlobal(pos));
    });

    // Wide enough for the longest label offered ("ISO-8859-1").
    unicodeType = new StatusLabel(90);
    unicodeType->setContentsMargins(6, 0, 6, 0);
    addPermanentWidget(unicodeType, 0);

    // Balances leftPadding: the labels' own 6px margin alone would leave the
    // last entry looking pinned to the window edge.
    auto *rightPadding = new QWidget(this);
    rightPadding->setFixedWidth(kEdgePadding - 6);
    addPermanentWidget(rightPadding, 0);

    applyChromeTheme();
}

void QsciEditorInfoStatusBar::refresh(QsciScintilla *editor)
{
    if (!editor) return;

    updateDocumentSize(editor);
    updateSelectionInfo(editor);
    updateLanguage(editor);
    updateEol(editor);
    updateEncoding(editor);
}

void QsciEditorInfoStatusBar::connectToEditor(QsciScintilla *editor)
{
    if (currentEditor)
    {
        disconnect(currentEditor, SIGNAL(cursorPositionChanged(int, int)), this, SLOT(editorCursorPositionChanged(int, int)));
        disconnect(currentEditor, SIGNAL(textChanged()), this, SLOT(editorTextChanged()));
        disconnect(currentEditor, SIGNAL(modificationChanged(bool)), this, SLOT(editorModificationChanged(bool)));
    }

    currentEditor = editor;
    if (editor)
    {
        connect(editor, SIGNAL(cursorPositionChanged(int, int)), this, SLOT(editorCursorPositionChanged(int, int)));
        connect(editor, SIGNAL(textChanged()), this, SLOT(editorTextChanged()));
        connect(editor, SIGNAL(modificationChanged(bool)), this, SLOT(editorModificationChanged(bool)));

        refresh(editor);
    }
}

void QsciEditorInfoStatusBar::editorCursorPositionChanged(int line, int index)
{
    Q_UNUSED(line);
    Q_UNUSED(index);

    if (currentEditor)
    {
        updateSelectionInfo(currentEditor);
    }
}

void QsciEditorInfoStatusBar::editorTextChanged()
{
    if (currentEditor)
    {
        updateDocumentSize(currentEditor);
        updateSelectionInfo(currentEditor);
    }
}

void QsciEditorInfoStatusBar::editorModificationChanged(bool modified)
{
    Q_UNUSED(modified);
    // No specific action needed for modification change in status bar
}

void QsciEditorInfoStatusBar::applyChromeTheme()
{
    using namespace ThemeColors::EditorTopBar;

    const QString styleSheet = QStringLiteral(
        "QStatusBar {"
        "  background-color: %1;"
        "  border-top: 1px solid %2;"
        "}"
        "QStatusBar::item { border: none; }"
    ).arg(background().name(QColor::HexRgb),
          border().name(QColor::HexRgb));

    // setStyleSheet() repolishes the widget and can synchronously emit another
    // PaletteChange. Avoid reapplying an identical sheet so that theme changes
    // remain safe on both Qt 5 and Qt 6.
    if (this->styleSheet() != styleSheet)
    {
        setStyleSheet(styleSheet);
    }
}

void QsciEditorInfoStatusBar::changeEvent(QEvent *event)
{
    QStatusBar::changeEvent(event);

    if (event->type() == QEvent::PaletteChange)
    {
        // Let QApplication finish installing the platform palette before QSS
        // repolishes this widget and its children.
        QTimer::singleShot(0, this, &QsciEditorInfoStatusBar::applyChromeTheme);
    }
}

void QsciEditorInfoStatusBar::updateDocumentSize(QsciScintilla *editor)
{
    QString sizeText = tr("Length: %1    Lines: %2")
                       .arg(editor->SendScintilla(QsciScintillaBase::SCI_GETLENGTH))
                       .arg(editor->lines());
    docSize->setText(sizeText);
}

void QsciEditorInfoStatusBar::updateSelectionInfo(QsciScintilla *editor)
{
    QString selectionText;
    int lineFrom, indexFrom, lineTo, indexTo;

    editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);

    if (editor->hasSelectedText())
    {
        const int start = editor->positionFromLineIndex(lineFrom, indexFrom);
        const int end = editor->positionFromLineIndex(lineTo, indexTo);
        const int chars = qMax(0, end - start);
        int lines = 1;

        // Count lines in selection
        if (lineTo > lineFrom)
        {
            lines = lineTo - lineFrom + 1;
        }

        selectionText = tr("Sel: %1 | %2").arg(chars).arg(lines);
    }
    else
    {
        selectionText = tr("Sel: 0 | 0");
    }

    int line, index;
    editor->getCursorPosition(&line, &index);
    QString positionText = tr("Ln: %1    Col: %2    ").arg(line + 1).arg(index + 1);
    docPos->setText(positionText + selectionText);
}

void QsciEditorInfoStatusBar::updateLanguage(QsciScintilla *editor)
{
    QsciLexer *lexer = editor->lexer();
    if (lexer)
    {
        docType->setText(lexer->language());
    }
    else
    {
        docType->setText(tr("Plain Text"));
    }
}

void QsciEditorInfoStatusBar::updateEol(QsciScintilla *editor)
{
    if (const auto *customEditor = qobject_cast<const CustomEditor *>(editor);
        customEditor && customEditor->documentFormat().lineEnding == DocumentLineEnding::Mixed)
    {
        eolFormat->setText(tr("Mixed line endings"));
        return;
    }

    switch(editor->eolMode())
    {
    case QsciScintilla::EolWindows:
        eolFormat->setText(tr("Windows (CR LF)"));
        break;
    case QsciScintilla::EolUnix:
        eolFormat->setText(tr("Unix (LF)"));
        break;
    case QsciScintilla::EolMac:
        eolFormat->setText(tr("Macintosh (CR)"));
        break;
    default:
        eolFormat->setText(tr("Unknown"));
        break;
    }
}

void QsciEditorInfoStatusBar::updateEncoding(QsciScintilla *editor)
{
    if (const auto *customEditor = qobject_cast<const CustomEditor *>(editor))
    {
        unicodeType->setText(
            DocumentFormatCodec::encodingName(customEditor->documentFormat().encoding));
    }
    else
    {
        unicodeType->setText(editor->isUtf8() ? tr("UTF-8") : tr("Unknown"));
    }
}
