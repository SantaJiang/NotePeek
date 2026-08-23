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

constexpr int kItemSpacing = 15;

} // namespace

QsciEditorInfoStatusBar::QsciEditorInfoStatusBar(QMainWindow *window)
    : QStatusBar(window), currentEditor(nullptr)
{
    setAttribute(Qt::WA_StyledBackground, true);

    gitStatus = new StatusLabel(this);
    gitStatus->setObjectName(QStringLiteral("statusGitRepository"));
    gitStatus->setCursor(Qt::PointingHandCursor);
    gitStatus->hide();
    addWidget(gitStatus, 0);
    connect(gitStatus, &StatusLabel::clicked,
            this, &QsciEditorInfoStatusBar::gitRefreshRequested);

    docPos = new StatusLabel(this);
    docPos->setObjectName(QStringLiteral("statusDocumentPosition"));
    addPermanentWidget(docPos, 0);

    auto addItemSpacing = [this](const QString &objectName) {
        auto *spacing = new QWidget(this);
        spacing->setObjectName(objectName);
        spacing->setFixedWidth(kItemSpacing);
        addPermanentWidget(spacing, 0);
    };

    addItemSpacing(QStringLiteral("statusSpacingAfterPosition"));

    // Size naturally so the compact group only occupies the space it needs.
    unicodeType = new StatusLabel(this);
    unicodeType->setObjectName(QStringLiteral("statusEncoding"));
    addPermanentWidget(unicodeType, 0);

    addItemSpacing(QStringLiteral("statusSpacingAfterEncoding"));

    eolFormat = new StatusLabel(this);
    eolFormat->setObjectName(QStringLiteral("statusLineEnding"));
    addPermanentWidget(eolFormat, 0);
    eolFormat->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(eolFormat, &StatusLabel::customContextMenuRequested, this, [=](const QPoint &pos)
    {
        emit customContextMenuRequestedForEOLLabel(eolFormat->mapToGlobal(pos));
    });

    addItemSpacing(QStringLiteral("statusSpacingAfterLineEnding"));

    docType = new StatusLabel(this);
    docType->setObjectName(QStringLiteral("statusDocumentType"));
    addPermanentWidget(docType, 0);

    applyChromeTheme();
}

void QsciEditorInfoStatusBar::setGitStatus(const QString &text, const QString &toolTip)
{
    gitStatus->setText(text);
    gitStatus->setToolTip(toolTip);
    gitStatus->setVisible(!text.isEmpty());
}

void QsciEditorInfoStatusBar::refresh(QsciScintilla *editor)
{
    if (!editor) return;

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
    else
    {
        docPos->clear();
        unicodeType->clear();
        eolFormat->clear();
        docType->clear();
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
        "  color: %3;"
        "  border-top: 1px solid %2;"
        "}"
        "QStatusBar QLabel { color: %3; }"
        "QStatusBar::item { border: none; }"
    ).arg(background().name(QColor::HexRgb),
          border().name(QColor::HexRgb),
          text().name(QColor::HexRgb));

    // setStyleSheet() repolishes the widget and can synchronously emit another
    // PaletteChange. Avoid reapplying an identical sheet during theme changes.
    if (this->styleSheet() != styleSheet)
    {
        setStyleSheet(styleSheet);
    }
}

void QsciEditorInfoStatusBar::reapplyTheme()
{
    applyChromeTheme();
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

void QsciEditorInfoStatusBar::updateSelectionInfo(QsciScintilla *editor)
{
    int line, index;
    editor->getCursorPosition(&line, &index);
    if (!editor->hasSelectedText())
    {
        docPos->setText(tr("Line %1, Column %2").arg(line + 1).arg(index + 1));
        return;
    }

    int lineFrom, indexFrom, lineTo, indexTo;
    editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    const int start = editor->positionFromLineIndex(lineFrom, indexFrom);
    const int end = editor->positionFromLineIndex(lineTo, indexTo);
    const int selectedCharacters = static_cast<int>(editor->SendScintilla(
        QsciScintillaBase::SCI_COUNTCHARACTERS, qMin(start, end), qMax(start, end)));
    docPos->setText(tr("Line %1, Column %2 (Selected %3)")
                        .arg(line + 1)
                        .arg(index + 1)
                        .arg(selectedCharacters));
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
