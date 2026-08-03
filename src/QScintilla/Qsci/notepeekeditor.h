// NotePeek editor extensions for QScintilla.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#ifndef NOTEPEEKEDITOR_H
#define NOTEPEEKEDITOR_H

#include <Qsci/qsciglobal.h>
#include <Qsci/qsciscintilla.h>
#include <NotePeek/semantictokens.h>

QT_BEGIN_NAMESPACE
class QKeyEvent;
QT_END_NAMESPACE

namespace NotePeek {

// Stable public editor surface supplied by the NotePeek QScintilla fork.  This class
// owns reusable editing mechanics only; file paths, encoding, persistence and
// application document state intentionally remain outside the library.
class QSCINTILLA_EXPORT Editor : public QsciScintilla
{
public:
    explicit Editor(QWidget *parent = nullptr);

    void setCodeEditingFeaturesEnabled(bool enabled);
    bool codeEditingFeaturesEnabled() const;

    void setBracketAssistanceEnabled(bool enabled);
    bool bracketAssistanceEnabled() const;

    void setDocumentCompletionEnabled(bool enabled);
    bool documentCompletionEnabled() const;

    void toggleCurrentFold();
    void toggleAllFolds();
    void goToMatchingBrace();
    void completeWordFromDocument();

    quint64 documentRevision() const;
    bool setSemanticTokens(quint64 revision, const QVector<SemanticToken> &tokens);
    bool applySemanticTokenDelta(quint64 revision, int firstLine, int lastLine,
                                 const QVector<SemanticToken> &tokens);
    void clearSemanticTokens();
    void setSemanticTokenColors(const SyntaxPalette &colors);
    QVector<SemanticToken> semanticTokens() const;
    int semanticTokenCount() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool m_codeEditingFeaturesEnabled = false;
    bool m_bracketAssistanceEnabled = false;
    bool m_documentCompletionEnabled = false;
    int m_semanticIndicator = 8;
    quint64 m_documentRevision = 0;
    QVector<SemanticToken> m_semanticTokens;
    SyntaxPalette m_semanticColors;

    void renderSemanticTokens();
    void renderSemanticToken(const SemanticToken &token);
    void handleSemanticModification(int modificationType, int linesAdded, int line);
    void clearSemanticIndicatorLines(int firstLine, int lastLine);
};

} // namespace NotePeek

#endif // NOTEPEEKEDITOR_H
