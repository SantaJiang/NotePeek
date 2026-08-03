#ifndef EDITORMANAGER_H
#define EDITORMANAGER_H

#include <QObject>
#include <QPointer>
#include <QList>
#include <QFont>
#include <QUrl>
#include <QtGlobal>
#include <memory>

class CustomEditor;
class SemanticHighlightingEngine;
class EditorManager : public QObject
{
    Q_OBJECT
public:
    explicit EditorManager(QObject *parent = nullptr);
    ~EditorManager();

    CustomEditor *createEditor(const QString &name = QString());
    CustomEditor *createEditorSharingDocument(CustomEditor *sourceEditor, const QString &name = QString());
    CustomEditor *createEditorForFile(const QString &filePath, bool tryToCreate = false);
    CustomEditor *getEditorByFilePath(const QString &filePath);
    QList<CustomEditor *> getEditors() const;
    void forgetEditor(CustomEditor *editor);

    void setupEditor(CustomEditor *editor);
    void updateLineNumberMarginWidth(CustomEditor *editor);
    void updateEditorSyntax(CustomEditor *editor, const QString &filePath);

    // Re-apply theme colours to every open editor (call on palette change)
    void reapplyThemeToAllEditors();

    // Re-apply the configured text/code fonts to every open editor (call after
    // the user changes font preferences).  Works for source editors with a
    // lexer attached, which ignore QsciScintilla::setFont().
    void applyFontPreferences();
    void applyCodeFeaturePreferences();

signals:
    void editorCreated(CustomEditor *editor);
    void editorClosed(CustomEditor *editor);
    void filesDropped(const QList<QUrl> &urls);

private:
    void setupScrollBarDecorator(CustomEditor *editor);
    // The text font for prose files, the code font for source files, as
    // configured in AppPreferences.
    QFont fontForPath(const QString &filePath) const;
    void setMarginsFont(CustomEditor *editor, const QFont &font);
    void applyDisplayPolicy(CustomEditor *editor, const QString &filePath);
    void purgeOldEditorPointers();
    void applyEditorColors(CustomEditor *editor);
    void applyLexerTheme(class QsciLexer *lexer);
    void applyUniformStyleFont(class QsciLexer *lexer, const QFont &font);
    void reapplyEditorSyntax(CustomEditor *editor);
    void applyBaseEditorColors(CustomEditor *editor);

    QList<QPointer<CustomEditor>> m_editors;
    std::unique_ptr<SemanticHighlightingEngine> m_semanticEngine;
};

#endif // EDITORMANAGER_H
