#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/editorcontentreplacer.h"
#include "editor/editorpane.h"
#include "foundation/performanceconstants.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QByteArray makeLineBasedContent(qsizetype targetSize)
{
    QByteArray content;
    content.reserve(targetSize + 64);
    while (content.size() < targetSize)
        content.append("0123456789 abcdefghijklmnopqrstuvwxyz\n");
    content.truncate(targetSize);
    return content;
}

int insertDeleteNotificationCount(const QSignalSpy &spy)
{
    int count = 0;
    for (const QList<QVariant> &args : spy)
    {
        const int modificationType = args.at(1).toInt();
        if (modificationType
            & (QsciScintillaBase::SC_MOD_INSERTTEXT | QsciScintillaBase::SC_MOD_DELETETEXT))
        {
            ++count;
        }
    }
    return count;
}

} // namespace

class EditorBulkLoadTest : public QObject
{
    Q_OBJECT

private slots:
    void bulkReplacementSuppressesInsertDeleteNotifications()
    {
        CustomEditor editor;
        editor.setReadOnly(true);
        QSignalSpy modifiedSpy(&editor, &QsciScintillaBase::SCN_MODIFIED);

        EditorContentReplacer replacer;
        const QByteArray content = makeLineBasedContent(8 * 1024 * 1024 + 1);
        bool finished = false;
        replacer.insertInChunks(&editor, content, [&finished]() { finished = true; });
        QTRY_VERIFY_WITH_TIMEOUT(finished, 15000);

        QCOMPARE(insertDeleteNotificationCount(modifiedSpy), 0);
        QCOMPARE(editor.length(), content.size());
    }

    void finishReplacementRestoresMaskUndoReadOnlyUpdates()
    {
        CustomEditor editor;
        editor.setReadOnly(true);
        editor.SendScintilla(QsciScintillaBase::SCI_SETMODEVENTMASK,
                             QsciScintillaBase::SC_MODEVENTMASKALL);
        editor.SendScintilla(QsciScintillaBase::SCI_SETUNDOCOLLECTION, 1UL);
        const long originalMask =
            editor.SendScintilla(QsciScintillaBase::SCI_GETMODEVENTMASK);

        EditorContentReplacer replacer;
        QVERIFY(replacer.beginReplacement(&editor));
        const long quietMask = editor.SendScintilla(QsciScintillaBase::SCI_GETMODEVENTMASK);
        QCOMPARE(int(quietMask & QsciScintillaBase::SC_MOD_INSERTTEXT), 0);
        QCOMPARE(int(quietMask & QsciScintillaBase::SC_MOD_DELETETEXT), 0);

        QVERIFY(replacer.appendChunk(&editor, "hello\nworld\n", 12));
        QVERIFY(replacer.finishReplacement(&editor));

        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_GETMODEVENTMASK), originalMask);
        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_GETUNDOCOLLECTION), long(1));
        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_GETMODIFY), long(0));
        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_CANUNDO), long(0));
        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_CANREDO), long(0));
        QVERIFY(editor.isReadOnly());
        QVERIFY(editor.updatesEnabled());
        QCOMPARE(editor.text(), QStringLiteral("hello\nworld\n"));
    }

    void normalEditAfterBulkLoadIsNotifiedAndUndoable()
    {
        CustomEditor editor;
        EditorContentReplacer replacer;
        QVERIFY(replacer.beginReplacement(&editor));
        QVERIFY(replacer.appendChunk(&editor, "before", 6));
        QVERIFY(replacer.finishReplacement(&editor));

        QSignalSpy modifiedSpy(&editor, &QsciScintillaBase::SCN_MODIFIED);
        editor.insert(QStringLiteral("x"));
        QCOMPARE(insertDeleteNotificationCount(modifiedSpy), 1);
        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_CANUNDO), long(1));
        editor.undo();
        QCOMPARE(editor.text(), QStringLiteral("before"));
    }

    void boundarySizesLoadIdentically()
    {
        const QList<qsizetype> sizes = {
            0,
            1,
            100,
            8 * 1024 * 1024,
            8 * 1024 * 1024 + 1,
        };

        for (qsizetype size : sizes)
        {
            CustomEditor editor;
            EditorContentReplacer replacer;
            const QByteArray content = makeLineBasedContent(size);
            bool finished = false;
            replacer.insertInChunks(&editor, content, [&finished]() { finished = true; });
            QTRY_VERIFY_WITH_TIMEOUT(finished, 15000);

            QCOMPARE(editor.length(), content.size());
            if (size <= 100)
            {
                QCOMPARE(editor.text(), QString::fromUtf8(content));
            }
            const qsizetype expectedLines = content.isEmpty() ? 1 : content.count('\n') + 1;
            QCOMPARE(editor.lines(), int(expectedLines));
        }
    }

    void abortReplacementDropsPartialContentAndRestoresState()
    {
        CustomEditor editor;
        editor.setReadOnly(true);
        const long originalMask = editor.SendScintilla(QsciScintillaBase::SCI_GETMODEVENTMASK);

        EditorContentReplacer replacer;
        QVERIFY(replacer.beginReplacement(&editor));
        QVERIFY(replacer.appendChunk(&editor, "partial content", 15));
        replacer.abortReplacement(&editor);

        QCOMPARE(editor.length(), 0);
        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_GETMODEVENTMASK), originalMask);
        QVERIFY(editor.isReadOnly());
        QVERIFY(editor.updatesEnabled());
        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_GETMODIFY), long(0));
    }

    void editorDestructionDuringReplacementIsSafe()
    {
        auto *editor = new CustomEditor;
        EditorContentReplacer replacer;
        QVERIFY(replacer.beginReplacement(editor));
        QVERIFY(replacer.appendChunk(editor, "doomed", 6));
        delete editor;
        replacer.forgetEditor(editor);
        // Reaching here without a crash or UAF is the assertion.
        QVERIFY(true);
    }

    void largeFileOpenEmitsSingleAggregatedSignals()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("large.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray content = makeLineBasedContent(8 * 1024 * 1024 + 1);
        QCOMPARE(file.write(content), qint64(content.size()));
        file.close();

        EditorPane pane;
        DocumentController controller(&pane, false);
        controller.openFile(path);

        CustomEditor *editor = controller.pane()->editorAt(0);
        QVERIFY(editor);
        QVERIFY(editor->isReadOnly());

        QSignalSpy textSpy(editor, &CustomEditor::textChanged);
        QSignalSpy linesSpy(editor, &CustomEditor::linesChanged);
        QSignalSpy openedSpy(&controller, &DocumentController::fileOpened);

        QTRY_COMPARE_WITH_TIMEOUT(openedSpy.count(), 1, 15000);

        QCOMPARE(textSpy.count(), 1);
        QCOMPARE(linesSpy.count(), 1);
        QCOMPARE(editor->documentRevision(), quint64(1));
        QCOMPARE(editor->length(), content.size());
        QCOMPARE(editor->SendScintilla(QsciScintillaBase::SCI_GETMODIFY), long(0));
        QVERIFY(!editor->isReadOnly());
    }

    void closingTabDuringLoadDoesNotCrash()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("large.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray content = makeLineBasedContent(16 * 1024 * 1024);
        QCOMPARE(file.write(content), qint64(content.size()));
        file.close();

        EditorPane pane;
        DocumentController controller(&pane, false);
        controller.openFile(path);
        // Close immediately, before the asynchronous load finishes.
        controller.requestClose(0);

        QTRY_COMPARE_WITH_TIMEOUT(controller.pane()->count(), 0, 15000);
        QVERIFY(true);
    }
};

QTEST_MAIN(EditorBulkLoadTest)
#include "editorbulkload_test.moc"
