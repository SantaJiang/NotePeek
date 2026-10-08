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
#include "widgets/iconfont.h"
#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <Qsci/qsciscintilla.h>
#include <Qsci/qsciscintillabase.h>
#include <Qsci/qscilexer.h>

namespace {

constexpr int kItemSpacing = 15;
constexpr int kGitIconSpacing = 5;
constexpr int kGitPopupGap = 6;

} // namespace

class GitStatusPopup : public QFrame
{
public:
    explicit GitStatusPopup(QWidget *parent = nullptr)
        : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
    {
        setObjectName(QStringLiteral("statusGitPopup"));
        setAttribute(Qt::WA_StyledBackground, true);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(8);

        m_details = new QLabel(this);
        m_details->setObjectName(QStringLiteral("statusGitPopupDetails"));
        m_details->setTextFormat(Qt::PlainText);
        m_details->setWordWrap(true);
        m_details->setMaximumWidth(420);
        layout->addWidget(m_details);

        m_refreshButton = new QPushButton(this);
        m_refreshButton->setObjectName(QStringLiteral("statusGitPopupRefresh"));
        m_refreshButton->setCursor(Qt::PointingHandCursor);
        layout->addWidget(m_refreshButton, 0, Qt::AlignLeft);
    }

    void setDetails(const QString &text)
    {
        m_details->setText(text);
    }

    void setRefreshText(const QString &text)
    {
        m_refreshButton->setText(text);
    }

    QPushButton *refreshButton() const
    {
        return m_refreshButton;
    }

    void applyChromeTheme()
    {
        using namespace ThemeColors::EditorTopBar;

        const QString foreground = text().name(QColor::HexRgb);
        const QString styleSheet = QStringLiteral(
            "QFrame#statusGitPopup {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 6px;"
            "}"
            "QLabel#statusGitPopupDetails {"
            "  color: %4;"
            "  background: transparent;"
            "}"
            "QPushButton#statusGitPopupRefresh {"
            "  color: %5;"
            "  background-color: %6;"
            "  border: none;"
            "  border-radius: 4px;"
            "  padding: 4px 10px;"
            "}"
            "QPushButton#statusGitPopupRefresh:hover {"
            "  background-color: %7;"
            "}"
        ).arg(popupBg().name(QColor::HexRgb),
              foreground,
              border().name(QColor::HexRgb),
              foreground,
              foreground,
              hoverBg().name(QColor::HexRgb),
              pressedBg().name(QColor::HexRgb));

        if (this->styleSheet() != styleSheet)
            setStyleSheet(styleSheet);
    }

private:
    QLabel *m_details = nullptr;
    QPushButton *m_refreshButton = nullptr;
};

QsciEditorInfoStatusBar::QsciEditorInfoStatusBar(QMainWindow *window)
    : QStatusBar(window), currentEditor(nullptr)
{
    setAttribute(Qt::WA_StyledBackground, true);
    // The platform size grip changes width (and disappears when maximized),
    // which makes the document information drift against the window edge.
    // Native window borders still provide resizing, so use explicit edge
    // spacers to keep the status-bar inset stable in every window state.
    setSizeGripEnabled(false);

    gitStatusContainer = new QWidget(this);
    gitStatusContainer->setObjectName(QStringLiteral("statusGitContainer"));
    auto *gitLayout = new QHBoxLayout(gitStatusContainer);
    gitLayout->setContentsMargins(0, 0, 0, 0);
    gitLayout->setSpacing(0);

    auto *gitLeftPadding = new QWidget(gitStatusContainer);
    gitLeftPadding->setObjectName(QStringLiteral("statusGitLeftPadding"));
    gitLeftPadding->setFixedWidth(kItemSpacing);
    gitLayout->addWidget(gitLeftPadding);

    gitBranchIcon = new StatusLabel(gitStatusContainer);
    gitBranchIcon->setObjectName(QStringLiteral("statusGitBranchIcon"));
    const QString &iconFontFamily = IconFont::fontAwesomeFamily();
    if (!iconFontFamily.isEmpty())
        gitBranchIcon->setFont(QFont(iconFontFamily));
    gitBranchIcon->setText(QChar(0xf126));
    gitBranchIcon->setCursor(Qt::PointingHandCursor);
    gitLayout->addWidget(gitBranchIcon);
    gitLayout->addSpacing(kGitIconSpacing);

    gitStatus = new StatusLabel(gitStatusContainer);
    gitStatus->setObjectName(QStringLiteral("statusGitRepository"));
    gitStatus->setCursor(Qt::PointingHandCursor);
    gitLayout->addWidget(gitStatus);
    gitStatusContainer->hide();
    addWidget(gitStatusContainer, 0);
    connect(gitBranchIcon, &StatusLabel::clicked,
            this, &QsciEditorInfoStatusBar::toggleGitDetailsPopup);
    connect(gitStatus, &StatusLabel::clicked,
            this, &QsciEditorInfoStatusBar::toggleGitDetailsPopup);

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

    addItemSpacing(QStringLiteral("statusRightEdgeSpacing"));

    applyChromeTheme();
}

void QsciEditorInfoStatusBar::setGitStatus(const QString &text, const QString &details)
{
    m_gitDetails = details;
    gitStatus->setText(text);
    gitStatus->setToolTip(QString());
    gitBranchIcon->setToolTip(QString());
    gitStatusContainer->setVisible(!text.isEmpty());

    if (!m_gitPopup)
        return;

    if (text.isEmpty() || details.isEmpty())
    {
        hideGitDetailsPopup();
        return;
    }

    if (m_gitPopup->isVisible())
    {
        m_gitPopup->setDetails(details);
        positionGitDetailsPopup();
    }
}

void QsciEditorInfoStatusBar::toggleGitDetailsPopup()
{
    if (m_gitDetails.isEmpty())
        return;

    if (!m_gitPopup)
    {
        m_gitPopup = new GitStatusPopup(this);
        m_gitPopup->installEventFilter(this);
        m_gitPopup->setRefreshText(tr("Refresh"));
        connect(m_gitPopup->refreshButton(), &QPushButton::clicked,
                this, &QsciEditorInfoStatusBar::gitRefreshRequested);
    }

    if (m_gitPopup->isVisible())
    {
        hideGitDetailsPopup();
        return;
    }

    // Qt::Popup closes on the mouse press that lands back on this control.
    // The matching release would otherwise open the bubble again immediately.
    if (m_suppressGitPopup)
    {
        m_suppressGitPopup = false;
        return;
    }

    m_gitPopup->setDetails(m_gitDetails);
    m_gitPopup->setRefreshText(tr("Refresh"));
    m_gitPopup->applyChromeTheme();
    positionGitDetailsPopup();
    m_gitPopup->show();
}

void QsciEditorInfoStatusBar::hideGitDetailsPopup()
{
    if (!m_gitPopup || !m_gitPopup->isVisible())
        return;

    m_hidingGitPopup = true;
    m_gitPopup->hide();
    m_hidingGitPopup = false;
}

void QsciEditorInfoStatusBar::positionGitDetailsPopup()
{
    if (!m_gitPopup)
        return;

    m_gitPopup->adjustSize();
    const QPoint anchor = gitStatusContainer->mapToGlobal(QPoint(0, 0));
    QPoint pos(anchor.x(), anchor.y() - m_gitPopup->height() - kGitPopupGap);
    if (QScreen *screen = gitStatusContainer->screen())
    {
        const QRect available = screen->availableGeometry();
        if (pos.x() + m_gitPopup->width() > available.right() - kGitPopupGap)
        {
            pos.setX(qMax(available.left() + kGitPopupGap,
                          available.right() - m_gitPopup->width() - kGitPopupGap));
        }
        if (pos.x() < available.left() + kGitPopupGap)
            pos.setX(available.left() + kGitPopupGap);
        if (pos.y() < available.top() + kGitPopupGap)
            pos.setY(anchor.y() + gitStatusContainer->height() + kGitPopupGap);
    }
    m_gitPopup->move(pos);
}

bool QsciEditorInfoStatusBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_gitPopup && event->type() == QEvent::Hide && !m_hidingGitPopup)
    {
        const QRect control(gitStatusContainer->mapToGlobal(QPoint(0, 0)),
                            gitStatusContainer->size());
        m_suppressGitPopup = QGuiApplication::mouseButtons() != Qt::NoButton
                             && control.contains(QCursor::pos());
    }
    return QStatusBar::eventFilter(watched, event);
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
    if (m_gitPopup)
        m_gitPopup->applyChromeTheme();
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
    else if (event->type() == QEvent::LanguageChange && m_gitPopup)
    {
        m_gitPopup->setRefreshText(tr("Refresh"));
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
