// Semantic syntax-style mapping for the NotePeek QScintilla fork.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#ifndef NOTEPEEK_SYNTAXSTYLER_H
#define NOTEPEEK_SYNTAXSTYLER_H

#include <Qsci/qsciglobal.h>

#include <QColor>
#include <QMap>

class QsciLexer;

namespace NotePeek {

enum class SyntaxRole
{
    Default,
    Comment,
    Documentation,
    Keyword,
    Type,
    Builtin,
    Function,
    Variable,
    Property,
    String,
    Escape,
    Number,
    Operator,
    Preprocessor,
    Regex,
    Attribute,
    Tag,
    Link,
    Added,
    Removed,
    Changed,
    Error
};

using SyntaxPalette = QMap<SyntaxRole, QColor>;

class QSCINTILLA_EXPORT SyntaxStyler
{
public:
    static SyntaxRole roleForStyle(const QsciLexer *lexer, int style);
    static void applyTheme(QsciLexer *lexer,
                           const SyntaxPalette &palette,
                           const QColor &paper,
                           const QColor &fallbackForeground);
};

} // namespace NotePeek

#endif // NOTEPEEK_SYNTAXSTYLER_H
