// Semantic-token API for the NotePeek QScintilla fork.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#ifndef NOTEPEEK_SEMANTICTOKENS_H
#define NOTEPEEK_SEMANTICTOKENS_H

#include <Qsci/qsciglobal.h>
#include <NotePeek/syntaxstyler.h>

#include <QString>
#include <QVector>
#include <atomic>
#include <memory>

namespace NotePeek {

// Positions use the Language Server Protocol convention: zero-based line and
// UTF-16 code-unit columns. The editor converts them to Scintilla UTF-8 bytes.
struct QSCINTILLA_EXPORT SemanticToken
{
    int line = 0;
    int utf16Column = 0;
    int utf16Length = 0;
    SyntaxRole role = SyntaxRole::Default;
};

struct QSCINTILLA_EXPORT SemanticDocument
{
    QString uri;
    QString languageId;
    QString text;
    quint64 revision = 0;
    int firstLine = 0;
};

struct QSCINTILLA_EXPORT SemanticAnalysisControl
{
    std::shared_ptr<std::atomic_bool> cancellation;
    qint64 budgetNanoseconds = 0;
};

struct QSCINTILLA_EXPORT SemanticAnalysisResult
{
    quint64 revision = 0;
    int firstLine = 0;
    int lastLine = -1;
    QVector<SemanticToken> tokens;
    qint64 elapsedNanoseconds = 0;
    bool canceled = false;
    bool budgetExceeded = false;
};

// Replaceable language-service boundary. MainPage can use the built-in fast
// classifier today and later plug in clangd, rust-analyzer or another LSP
// client without changing the editor widget or its theme machinery.
class QSCINTILLA_EXPORT SemanticTokenProvider
{
public:
    virtual ~SemanticTokenProvider() = default;
    virtual SemanticAnalysisResult analyze(
        const SemanticDocument &document,
        const SemanticAnalysisControl &control = {}) const = 0;

    QVector<SemanticToken> tokensForDocument(const SemanticDocument &document) const
    {
        return analyze(document).tokens;
    }
};

class QSCINTILLA_EXPORT BasicSemanticTokenProvider final : public SemanticTokenProvider
{
public:
    SemanticAnalysisResult analyze(
        const SemanticDocument &document,
        const SemanticAnalysisControl &control = {}) const override;
};

} // namespace NotePeek

#endif // NOTEPEEK_SEMANTICTOKENS_H
