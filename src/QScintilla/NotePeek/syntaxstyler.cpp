// Semantic syntax-style mapping for the NotePeek QScintilla fork.
// Copyright (c) 2026 The NotePeek contributors.
// Distributed under the GNU General Public License version 3.0.

#include "NotePeek/syntaxstyler.h"

#include <Qsci/qscilexer.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerrust.h>
#include <Qsci/qsciscintillabase.h>

namespace {

using Role = NotePeek::SyntaxRole;

Role roleFromDescription(QString description)
{
    description = description.toLower();
    if (description.contains(QLatin1String("error")) || description.contains(QLatin1String("unknown"))
        || description.contains(QLatin1String("unclosed")) || description.contains(QLatin1String("bad")))
        return Role::Error;
    if (description.contains(QLatin1String("comment")) || description.contains(QLatin1String("remark")))
        return description.contains(QLatin1String("doc")) ? Role::Documentation : Role::Comment;
    if (description.contains(QLatin1String("escape")) || description.contains(QLatin1String("entity")))
        return Role::Escape;
    if (description.contains(QLatin1String("regex")) || description.contains(QLatin1String("regular expression")))
        return Role::Regex;
    if (description.contains(QLatin1String("string")) || description.contains(QLatin1String("character"))
        || description.contains(QLatin1String("quoted")))
        return Role::String;
    if (description.contains(QLatin1String("number")) || description.contains(QLatin1String("numeric")))
        return Role::Number;
    if (description.contains(QLatin1String("preprocessor")) || description.contains(QLatin1String("directive"))
        || description.contains(QLatin1String("@-rule")))
        return Role::Preprocessor;
    if (description.contains(QLatin1String("operator")) || description.contains(QLatin1String("delimiter")))
        return Role::Operator;
    if (description.contains(QLatin1String("function")) || description.contains(QLatin1String("method"))
        || description.contains(QLatin1String("command")) || description.contains(QLatin1String("target"))
        || description.contains(QLatin1String("label")))
        return Role::Function;
    if (description.contains(QLatin1String("class name")) || description.contains(QLatin1String("type"))
        || description.contains(QLatin1String("lifetime")))
        return Role::Type;
    if (description.contains(QLatin1String("builtin")) || description.contains(QLatin1String("built-in"))
        || description.contains(QLatin1String("macro")))
        return Role::Builtin;
    if (description.contains(QLatin1String("property")) || description.contains(QLatin1String("key"))
        || description.contains(QLatin1String("section")))
        return Role::Property;
    if (description.contains(QLatin1String("attribute")) || description.contains(QLatin1String("decorator")))
        return Role::Attribute;
    if (description.contains(QLatin1String("tag")) || description.contains(QLatin1String("heading")))
        return Role::Tag;
    if (description.contains(QLatin1String("link")) || description.contains(QLatin1String("url"))
        || description.contains(QLatin1String("iri")))
        return Role::Link;
    if (description.contains(QLatin1String("keyword")) || description.contains(QLatin1String("reserved")))
        return Role::Keyword;
    if (description.contains(QLatin1String("variable")) || description.contains(QLatin1String("parameter"))
        || description.contains(QLatin1String("reference")))
        return Role::Variable;
    return Role::Default;
}

Role roleForKnownLexer(const QsciLexer *lexer, int style)
{
    if (dynamic_cast<const QsciLexerDiff *>(lexer)) {
        switch (style) {
        case QsciLexerDiff::Comment: return Role::Comment;
        case QsciLexerDiff::Command: case QsciLexerDiff::Header: return Role::Keyword;
        case QsciLexerDiff::Position: return Role::Link;
        case QsciLexerDiff::LineAdded: case QsciLexerDiff::AddingPatchAdded:
        case QsciLexerDiff::RemovingPatchAdded: return Role::Added;
        case QsciLexerDiff::LineRemoved: case QsciLexerDiff::AddingPatchRemoved:
        case QsciLexerDiff::RemovingPatchRemoved: return Role::Removed;
        case QsciLexerDiff::LineChanged: return Role::Changed;
        default: break;
        }
    }
    if (dynamic_cast<const QsciLexerMarkdown *>(lexer)) {
        switch (style) {
        case QsciLexerMarkdown::StrongEmphasisAsterisks:
        case QsciLexerMarkdown::StrongEmphasisUnderscores: return Role::Keyword;
        case QsciLexerMarkdown::EmphasisAsterisks:
        case QsciLexerMarkdown::EmphasisUnderscores:
        case QsciLexerMarkdown::StrikeOut: return Role::Attribute;
        case QsciLexerMarkdown::Header1: case QsciLexerMarkdown::Header2:
        case QsciLexerMarkdown::Header3: case QsciLexerMarkdown::Header4:
        case QsciLexerMarkdown::Header5: case QsciLexerMarkdown::Header6: return Role::Tag;
        case QsciLexerMarkdown::UnorderedListItem: case QsciLexerMarkdown::OrderedListItem:
        case QsciLexerMarkdown::HorizontalRule: return Role::Operator;
        case QsciLexerMarkdown::BlockQuote: return Role::Documentation;
        case QsciLexerMarkdown::Link: return Role::Link;
        case QsciLexerMarkdown::CodeBackticks: case QsciLexerMarkdown::CodeDoubleBackticks:
        case QsciLexerMarkdown::CodeBlock: return Role::String;
        default: break;
        }
    }
    if (dynamic_cast<const QsciLexerPython *>(lexer)) {
        switch (style) {
        case QsciLexerPython::Comment: case QsciLexerPython::CommentBlock: return Role::Comment;
        case QsciLexerPython::Number: return Role::Number;
        case QsciLexerPython::DoubleQuotedString: case QsciLexerPython::SingleQuotedString:
        case QsciLexerPython::TripleSingleQuotedString: case QsciLexerPython::TripleDoubleQuotedString:
        case QsciLexerPython::DoubleQuotedFString: case QsciLexerPython::SingleQuotedFString:
        case QsciLexerPython::TripleSingleQuotedFString: case QsciLexerPython::TripleDoubleQuotedFString:
            return Role::String;
        case QsciLexerPython::Keyword: return Role::Keyword;
        case QsciLexerPython::ClassName: return Role::Type;
        case QsciLexerPython::FunctionMethodName: return Role::Function;
        case QsciLexerPython::Operator: return Role::Operator;
        case QsciLexerPython::Identifier: return Role::Variable;
        case QsciLexerPython::HighlightedIdentifier: return Role::Builtin;
        case QsciLexerPython::Decorator: return Role::Attribute;
        case QsciLexerPython::UnclosedString: return Role::Error;
        default: break;
        }
    }
    if (dynamic_cast<const QsciLexerRust *>(lexer)) {
        switch (style) {
        case QsciLexerRust::CommentBlock: case QsciLexerRust::CommentLine: return Role::Comment;
        case QsciLexerRust::CommentBlockDoc: case QsciLexerRust::CommentLineDoc: return Role::Documentation;
        case QsciLexerRust::Number: return Role::Number;
        case QsciLexerRust::Keyword: case QsciLexerRust::KeywordSet3: case QsciLexerRust::KeywordSet4:
        case QsciLexerRust::KeywordSet5: case QsciLexerRust::KeywordSet6: case QsciLexerRust::KeywordSet7:
            return Role::Keyword;
        case QsciLexerRust::BuiltInType: case QsciLexerRust::Lifetime: return Role::Type;
        case QsciLexerRust::String: case QsciLexerRust::RawString: case QsciLexerRust::Character:
        case QsciLexerRust::ByteString: case QsciLexerRust::RawByteString: case QsciLexerRust::ByteCharacter:
            return Role::String;
        case QsciLexerRust::Operator: return Role::Operator;
        case QsciLexerRust::Identifier: return Role::Variable;
        case QsciLexerRust::Macro: return Role::Builtin;
        case QsciLexerRust::LexingError: return Role::Error;
        default: break;
        }
    }
    if (dynamic_cast<const QsciLexerJSON *>(lexer)) {
        switch (style) {
        case QsciLexerJSON::Number: return Role::Number;
        case QsciLexerJSON::String: return Role::String;
        case QsciLexerJSON::Property: return Role::Property;
        case QsciLexerJSON::EscapeSequence: return Role::Escape;
        case QsciLexerJSON::CommentLine: case QsciLexerJSON::CommentBlock: return Role::Comment;
        case QsciLexerJSON::Operator: return Role::Operator;
        case QsciLexerJSON::IRI: case QsciLexerJSON::IRICompact: return Role::Link;
        case QsciLexerJSON::Keyword: case QsciLexerJSON::KeywordLD: return Role::Keyword;
        case QsciLexerJSON::UnclosedString: case QsciLexerJSON::Error: return Role::Error;
        default: break;
        }
    }
    if (dynamic_cast<const QsciLexerCPP *>(lexer)) {
        const int activeStyle = style >= 64 ? style - 64 : style;
        switch (activeStyle) {
        case QsciLexerCPP::Comment: case QsciLexerCPP::CommentLine: return Role::Comment;
        case QsciLexerCPP::CommentDoc: case QsciLexerCPP::CommentLineDoc:
        case QsciLexerCPP::CommentDocKeyword: case QsciLexerCPP::TaskMarker: return Role::Documentation;
        case QsciLexerCPP::CommentDocKeywordError: case QsciLexerCPP::UnclosedString: return Role::Error;
        case QsciLexerCPP::Number: return Role::Number;
        case QsciLexerCPP::Keyword: return Role::Keyword;
        case QsciLexerCPP::KeywordSet2: case QsciLexerCPP::GlobalClass: return Role::Type;
        case QsciLexerCPP::DoubleQuotedString: case QsciLexerCPP::SingleQuotedString:
        case QsciLexerCPP::VerbatimString: case QsciLexerCPP::RawString:
        case QsciLexerCPP::TripleQuotedVerbatimString: case QsciLexerCPP::HashQuotedString:
        case QsciLexerCPP::UserLiteral: return Role::String;
        case QsciLexerCPP::EscapeSequence: return Role::Escape;
        case QsciLexerCPP::Regex: return Role::Regex;
        case QsciLexerCPP::PreProcessor: return Role::Preprocessor;
        case QsciLexerCPP::PreProcessorComment: return Role::Comment;
        case QsciLexerCPP::PreProcessorCommentLineDoc: return Role::Documentation;
        case QsciLexerCPP::Operator: return Role::Operator;
        case QsciLexerCPP::Identifier: return Role::Default;
        default: break;
        }
    }
    return roleFromDescription(lexer->description(style));
}

} // namespace

namespace NotePeek {

SyntaxRole SyntaxStyler::roleForStyle(const QsciLexer *lexer, int style)
{
    return lexer ? roleForKnownLexer(lexer, style) : SyntaxRole::Default;
}

void SyntaxStyler::applyTheme(QsciLexer *lexer,
                              const SyntaxPalette &palette,
                              const QColor &paper,
                              const QColor &fallbackForeground)
{
    if (!lexer)
        return;

    lexer->setDefaultPaper(paper);
    lexer->setPaper(paper, -1);
    for (int style = 0; style <= QsciScintillaBase::STYLE_MAX; ++style)
    {
        if (lexer->description(style).isEmpty())
            continue;
        lexer->setColor(palette.value(roleForStyle(lexer, style), fallbackForeground), style);
    }
}

} // namespace NotePeek
