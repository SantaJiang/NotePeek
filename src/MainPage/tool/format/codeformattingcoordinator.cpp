#include "tool/format/codeformattingcoordinator.h"

#include "foundation/backgroundworkscheduler.h"
#include "editor/customeditor.h"
#include "tool/format/codeformatter.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QPointer>
#include <QtConcurrent>

#include <utility>

namespace {

QString eolText(QsciScintilla::EolMode mode)
{
    switch (mode)
    {
    case QsciScintilla::EolWindows:
        return QStringLiteral("\r\n");
    case QsciScintilla::EolMac:
        return QStringLiteral("\r");
    case QsciScintilla::EolUnix:
        break;
    }
    return QStringLiteral("\n");
}

} // namespace

CodeFormattingCoordinator::CodeFormattingCoordinator(
    QWidget *dialogParent,
    FocusEligibility focusEligibility,
    QObject *parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
    , m_focusEligibility(std::move(focusEligibility))
{
}

void CodeFormattingCoordinator::format(CustomEditor *editor)
{
    if (!editor || editor->isReadOnly()
        || editor->property("notepeekFormattingInProgress").toBool())
    {
        return;
    }

    const QString filePath = editor->filePath();
    if (!CodeFormatter::canFormat(filePath))
        return;

    CodeFormatter::Options options;
    options.indentWidth = editor->indentationWidth() > 0 ? editor->indentationWidth()
                                                         : editor->tabWidth();
    options.useTabs = editor->indentationsUseTabs();
    options.eol = eolText(editor->eolMode());

    const QString original = editor->text();
    const quint64 revision = editor->documentRevision();
    QPointer<CustomEditor> editorGuard(editor);
    editor->setProperty("notepeekFormattingInProgress", true);
    emit formattingStateChanged();

    auto *watcher = new QFutureWatcher<CodeFormatter::Result>(this);
    connect(watcher, &QFutureWatcher<CodeFormatter::Result>::finished,
            this, [this, watcher, editorGuard, revision, original, filePath]() {
        const CodeFormatter::Result result = watcher->result();
        watcher->deleteLater();
        if (editorGuard)
            editorGuard->setProperty("notepeekFormattingInProgress", false);
        emit formattingStateChanged();

        CustomEditor *editor = editorGuard.data();
        if (!editor || editor->documentRevision() != revision
            || editor->filePath() != filePath)
        {
            return;
        }
        if (!result.ok)
        {
            QMessageBox::warning(
                m_dialogParent,
                QCoreApplication::translate("MainWindow", "Format Code"),
                QCoreApplication::translate("MainWindow", "Could not format %1.\n\n%2")
                    .arg(QFileInfo(filePath).fileName(), result.error));
            return;
        }

        // Avoid an empty undo step and a false modified state.
        if (result.text == original)
            return;

        int line = 0;
        int index = 0;
        editor->getCursorPosition(&line, &index);
        const int firstVisibleLine = int(editor->SendScintilla(
            QsciScintillaBase::SCI_GETFIRSTVISIBLELINE));
        const int xOffset = int(editor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET));

        // Keep the replacement as one undoable action; setText() would clear
        // the undo buffer.
        const bool updatesWereEnabled = editor->updatesEnabled();
        editor->setUpdatesEnabled(false);
        editor->beginUndoAction();
        editor->selectAll();
        editor->replaceSelectedText(result.text);
        editor->endUndoAction();
        editor->setUpdatesEnabled(updatesWereEnabled);

        const int targetLine = qBound(0, line, qMax(0, editor->lines() - 1));
        editor->setCursorPosition(targetLine,
                                  qBound(0, index, editor->lineLength(targetLine)));
        editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
        editor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);
        editor->viewport()->update();
        if (m_focusEligibility && m_focusEligibility(editor))
            editor->setFocus();
    });
    watcher->setFuture(QtConcurrent::run(
        BackgroundWorkScheduler::interactivePool(),
        [original, filePath, options]() {
            return CodeFormatter::format(original, filePath, options);
        }));
}
