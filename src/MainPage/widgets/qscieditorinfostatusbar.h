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

#ifndef QSCIEDITORINFOSTATUSBAR_H
#define QSCIEDITORINFOSTATUSBAR_H

#include <QStatusBar>
#include <QMainWindow>
#include <QLabel>
#include <QMouseEvent>
#include <QEvent>
#include <QPointer>


class QsciScintilla;
class ClickableLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ClickableLabel(QWidget *parent=Q_NULLPTR, Qt::WindowFlags f=Qt::WindowFlags()) : QLabel(parent, f) {}
    explicit ClickableLabel(const QString &text, QWidget *parent=Q_NULLPTR, Qt::WindowFlags f=Qt::WindowFlags()) : QLabel(text, parent, f) {}

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override { Q_UNUSED(event); emit clicked(); }
    void mouseDoubleClickEvent(QMouseEvent *event) override { emit customContextMenuRequested(event->pos()); }
};


class StatusLabel : public ClickableLabel
{
    Q_OBJECT

public:
    explicit StatusLabel(QWidget *parent = nullptr) : ClickableLabel(parent) {
        // Since these get set a lot and plain text is always used go ahead and set the format
        setTextFormat(Qt::PlainText);
    }
};

class QLabel;
class QsciEditorInfoStatusBar : public QStatusBar
{
    Q_OBJECT

public:
    explicit QsciEditorInfoStatusBar(QMainWindow *window);
    void refresh(QsciScintilla *editor);
    void connectToEditor(QsciScintilla *editor);
    void reapplyTheme();
    void setGitStatus(const QString &text, const QString &toolTip = QString());

signals:
    void customContextMenuRequestedForEOLLabel(const QPoint &pos);
    void gitRefreshRequested();

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void editorCursorPositionChanged(int line, int index);
    void editorTextChanged();
    void editorModificationChanged(bool modified);

private:
    void updateSelectionInfo(QsciScintilla *editor);
    void updateLanguage(QsciScintilla *editor);
    void updateEol(QsciScintilla *editor);
    void updateEncoding(QsciScintilla *editor);
    void applyChromeTheme();

    QLabel *docType;
    QLabel *docPos;
    QLabel *eolFormat;
    QLabel *unicodeType;
    StatusLabel *gitStatus;

    QPointer<QsciScintilla> currentEditor;
};

#endif // QSCIEDITORINFOSTATUSBAR_H
