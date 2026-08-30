#include "editor/customeditor.h"
#include "editor/editorcontentreplacer.h"
#include "editor/editormanager.h"
#include "foundation/performanceconstants.h"

#include <Qsci/qscidocument.h>

#include <QTemporaryDir>
#include <QtTest>

namespace {

bool hasStylesNone(const CustomEditor &editor)
{
    return (editor.SendScintilla(QsciScintillaBase::SCI_GETDOCUMENTOPTIONS)
            & QsciScintillaBase::SC_DOCUMENTOPTION_STYLES_NONE) != 0;
}

QString makeSparseFile(QTemporaryDir &dir, const QString &name, qint64 size, QByteArray *contents = nullptr)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return QString();
    if (size > 0)
    {
        if (contents)
        {
            file.write(*contents);
        }
        else
        {
            file.seek(size - 1);
            file.write("x", 1);
        }
    }
    file.close();
    return path;
}

} // namespace

class DocumentOptionsTest : public QObject
{
    Q_OBJECT

private slots:
    void largeFileEditorUsesStylelessDocument()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = makeSparseFile(dir, QStringLiteral("large.log"),
                                            PerformanceConstants::LargeFileSize + 1);

        EditorManager manager;
        CustomEditor *editor = manager.createEditorForFile(path, false);
        QVERIFY(editor);
        QVERIFY(editor->isLargeFileMode());
        QVERIFY(hasStylesNone(*editor));
        QVERIFY(editor->lexer() == nullptr);
    }

    void thresholdBoundarySelectsDocumentOptions()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        EditorManager manager;
        const QString exactThreshold = makeSparseFile(dir, QStringLiteral("exact.log"),
                                                      PerformanceConstants::LargeFileSize);
        CustomEditor *small = manager.createEditorForFile(exactThreshold, false);
        QVERIFY(small);
        QVERIFY(!small->isLargeFileMode());
        QVERIFY(!hasStylesNone(*small));

        const QString justOver = makeSparseFile(dir, QStringLiteral("over.log"),
                                                PerformanceConstants::LargeFileSize + 1);
        CustomEditor *large = manager.createEditorForFile(justOver, false);
        QVERIFY(large);
        QVERIFY(large->isLargeFileMode());
        QVERIFY(hasStylesNone(*large));
    }

    void stylelessDocumentLoadsSearchesAndQueriesSafely()
    {
        CustomEditor editor;
        editor.setDocument(QsciDocument(QsciScintillaBase::SC_DOCUMENTOPTION_STYLES_NONE));
        QVERIFY(hasStylesNone(editor));

        EditorContentReplacer replacer;
        QVERIFY(replacer.beginReplacement(&editor));
        QVERIFY(replacer.appendChunk(&editor, "alpha\nbeta\ngamma\n", 17));
        QVERIFY(replacer.finishReplacement(&editor));
        editor.notifyDocumentReplaced(true);

        QCOMPARE(editor.length(), 17);
        QCOMPARE(editor.lines(), 4);
        QCOMPARE(editor.text(), QStringLiteral("alpha\nbeta\ngamma\n"));
        // Style queries keep the default (style 0) semantics and do not crash.
        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_GETSTYLEAT, 0L), long(0));
        QCOMPARE(editor.SendScintilla(QsciScintillaBase::SCI_GETMODIFY), long(0));
        QVERIFY(editor.findFirst(QStringLiteral("beta"), false, false, false, true, true, 0, 0));
    }

    void stylelessDocumentNeverGetsALexer()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = makeSparseFile(dir, QStringLiteral("large.cpp"),
                                            PerformanceConstants::LargeFileSize + 1);

        EditorManager manager;
        CustomEditor *editor = manager.createEditorForFile(path, false);
        QVERIFY(editor);

        // Re-apply syntax (theme / preferences / Save As / reload) and confirm
        // the lexer is never re-attached to a styleless document.
        manager.updateEditorSyntax(editor, path);
        QVERIFY(editor->lexer() == nullptr);
        QVERIFY(hasStylesNone(*editor));
        manager.updateEditorSyntax(editor, QStringLiteral("renamed.py"));
        QVERIFY(editor->lexer() == nullptr);
        QVERIFY(hasStylesNone(*editor));
    }

    void splitViewSharesOneStylelessDocument()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = makeSparseFile(dir, QStringLiteral("large.log"),
                                            PerformanceConstants::LargeFileSize + 1);

        EditorManager manager;
        CustomEditor *editor = manager.createEditorForFile(path, false);
        CustomEditor *split = manager.createEditorSharingDocument(editor, QStringLiteral("split"));
        QVERIFY(split);
        QVERIFY(hasStylesNone(*split));
        QCOMPARE(split->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER),
                 editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER));
    }

    void stylelessDocumentCopyAndDestroyIsSafe()
    {
        auto *first = new CustomEditor;
        QsciDocument styleless(QsciScintillaBase::SC_DOCUMENTOPTION_STYLES_NONE);
        first->setDocument(styleless);
        QVERIFY(hasStylesNone(*first));

        QsciDocument copy = first->document();
        auto *second = new CustomEditor;
        second->setDocument(copy);
        QCOMPARE(second->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER),
                 first->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER));

        // Destroying the views in either order must not crash or double-free.
        delete first;
        delete second;
        QVERIFY(true);
    }
};

QTEST_MAIN(DocumentOptionsTest)
#include "documentoptions_test.moc"
