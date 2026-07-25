#ifndef THEMECOLORS_H
#define THEMECOLORS_H

#include <QColor>
#include <QPalette>
#include <QGuiApplication>
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

//
// Centralised dark / light colour definitions for NotePeek.
// Every component that needs theme-aware colours should include this header
// instead of hard-coding RGB values.
//
// The values live in one data table (roleColors) keyed by ColorRole, with a
// light and a dark value per role; the per-widget accessor functions are thin
// wrappers over it. A future user-selectable theme only needs to substitute
// the table lookup, not the call sites.
//
namespace ThemeColors {

// ---------- detection ----------

inline bool isDarkMode()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    const QPalette pal = QGuiApplication::palette();
    return pal.color(QPalette::Window).lightness() < 128;
#endif
}

// ---------- palette data ----------

enum class ColorRole
{
    // Editor (QScintilla)
    EditorMarginBg,
    EditorMarginFg,
    EditorCaretLineBg,
    EditorCaretFg,
    EditorBackground,
    EditorForeground,
    EditorIndentGuide,
    EditorFoldMarker,
    EditorMatchedBraceFg,
    EditorMatchedBraceBg,
    // Lexer syntax highlighting
    LexerDefaultPaper,
    LexerDefaultColor,
    LexerKeyword,
    LexerString,
    LexerComment,
    LexerNumber,
    LexerPreprocessor,
    LexerOperator,
    LexerIdentifier,
    LexerDocKeyword,
    LexerType,
    LexerDocumentation,
    LexerBuiltin,
    LexerFunction,
    LexerVariable,
    LexerProperty,
    LexerEscape,
    LexerRegex,
    LexerAttribute,
    LexerTag,
    LexerLink,
    LexerAdded,
    LexerRemoved,
    LexerChanged,
    LexerError,
    // Scroll-bar decorator
    ScrollBarSearchResult,
    ScrollBarBookmark,
    ScrollBarCurrentLine,
    ScrollBarSelection,
    // Find / replace
    FindReplaceHighlight,
    // Editor splitter
    SplitterHandleBg,
    SplitterHandleBorder,
    // Editor pane top bar
    TopBarBackground,
    TopBarBorder,
    TopBarText,
    TopBarMutedText,
    TopBarHoverBg,
    TopBarPressedBg,
    TopBarPopupBg,
    TopBarSelectionBg,
    TopBarSelectionText,
    // File tree
    FileTreeBackground,
    FileTreeText,
    FileTreeSelectionBg,
    FileTreeSelectionText,
    FileTreeHoverBg,
    FileTreeBorder,
    // Editor-switch popup
    SwitchPopupBorder,
    SwitchPopupBackground,
    SwitchPopupSelectedItemBg,
    SwitchPopupSelectedItemFg,
    SwitchPopupText,
    // Preferences dialog
    PrefWindowBg,
    PrefPanelBg,
    PrefInputBg,
    PrefBorder,
    PrefText,
    PrefMutedText,
    PrefHoverBg,
    PrefPressedBg,
    PrefAccent,
    PrefAccentPressed,
    PrefAccentText,
    // Markdown preview
    MarkdownBodyBg,
    MarkdownBodyFg,
    MarkdownHeadingFg,
    MarkdownHeadingBorder,
    MarkdownCodeBg,
    MarkdownCodeBorder,
    MarkdownBlockquoteBorder,
    MarkdownBlockquoteFg,
    MarkdownTableBorder,
    MarkdownLinkFg
};

struct RoleColors
{
    QRgb light;
    QRgb dark;
};

// The single source of truth for every themable colour. The switch (instead
// of an index-ordered array) lets the compiler flag missing roles.
inline RoleColors roleColors(ColorRole role)
{
    switch (role)
    {
    case ColorRole::EditorMarginBg:          return {qRgb(0xf0,0xf0,0xf0), qRgb(0x2d,0x2d,0x2d)};
    case ColorRole::EditorMarginFg:          return {qRgb(0x66,0x66,0x66), qRgb(0x85,0x85,0x85)};
    case ColorRole::EditorCaretLineBg:       return {qRgb(0xe8,0xe8,0xff), qRgb(0x2a,0x2a,0x2a)};
    case ColorRole::EditorCaretFg:           return {qRgb(0x00,0x00,0x00), qRgb(0xff,0xff,0xff)};
    case ColorRole::EditorBackground:        return {qRgb(0xff,0xff,0xff), qRgb(0x1e,0x1e,0x1e)};
    case ColorRole::EditorForeground:        return {qRgb(0x00,0x00,0x00), qRgb(0xd4,0xd4,0xd4)};
    case ColorRole::EditorIndentGuide:       return {qRgb(0xd3,0xd3,0xd3), qRgb(0x40,0x40,0x40)};
    case ColorRole::EditorFoldMarker:        return {qRgb(0x6b,0x6b,0x6b), qRgb(0x8a,0x8a,0x8a)};
    case ColorRole::EditorMatchedBraceFg:    return {qRgb(0x00,0x00,0xff), qRgb(0x56,0x9c,0xd6)};
    case ColorRole::EditorMatchedBraceBg:    return {qRgb(0xff,0xff,0x00), qRgb(0x3b,0x3b,0x3b)};

    case ColorRole::LexerDefaultPaper:       return {qRgb(0xff,0xff,0xff), qRgb(0x1e,0x1e,0x1e)};
    case ColorRole::LexerDefaultColor:       return {qRgb(0x00,0x00,0x00), qRgb(0xd4,0xd4,0xd4)};
    case ColorRole::LexerKeyword:            return {qRgb(0x00,0x00,0xff), qRgb(0x56,0x9c,0xd6)};
    case ColorRole::LexerString:             return {qRgb(0xa3,0x15,0x15), qRgb(0xce,0x91,0x78)};
    case ColorRole::LexerComment:            return {qRgb(0x00,0x80,0x00), qRgb(0x6a,0x99,0x55)};
    case ColorRole::LexerNumber:             return {qRgb(0x00,0x00,0x80), qRgb(0xb5,0xce,0xa8)};
    case ColorRole::LexerPreprocessor:       return {qRgb(0x80,0x00,0x80), qRgb(0xc5,0x86,0xc0)};
    case ColorRole::LexerOperator:           return {qRgb(0x00,0x00,0x00), qRgb(0xd4,0xd4,0xd4)};
    case ColorRole::LexerIdentifier:         return {qRgb(0x00,0x00,0x00), qRgb(0xdc,0xdc,0xdc)};
    case ColorRole::LexerDocKeyword:         return {qRgb(0x00,0x80,0x80), qRgb(0x4e,0xc9,0xb0)};
    case ColorRole::LexerType:               return {qRgb(0x2b,0x91,0xaf), qRgb(0x4e,0xc9,0xb0)};
    case ColorRole::LexerDocumentation:      return {qRgb(0x3f,0x7f,0x5f), qRgb(0x60,0x8b,0x4e)};
    case ColorRole::LexerBuiltin:            return {qRgb(0x79,0x50,0x0d), qRgb(0xd7,0xba,0x7d)};
    case ColorRole::LexerFunction:           return {qRgb(0x79,0x50,0x0d), qRgb(0xdc,0xdc,0xaa)};
    case ColorRole::LexerVariable:           return {qRgb(0x00,0x16,0x7c), qRgb(0x9c,0xdc,0xfe)};
    case ColorRole::LexerProperty:           return {qRgb(0x88,0x10,0x00), qRgb(0x9c,0xdc,0xfe)};
    case ColorRole::LexerEscape:             return {qRgb(0xee,0x00,0x00), qRgb(0xd7,0xba,0x7d)};
    case ColorRole::LexerRegex:              return {qRgb(0x81,0x1f,0x3f), qRgb(0xd1,0x69,0x69)};
    case ColorRole::LexerAttribute:          return {qRgb(0x99,0x1f,0x66), qRgb(0x9c,0xdc,0xfe)};
    case ColorRole::LexerTag:                return {qRgb(0x80,0x00,0x00), qRgb(0x56,0x9c,0xd6)};
    case ColorRole::LexerLink:               return {qRgb(0x00,0x00,0xee), qRgb(0x4f,0xc1,0xff)};
    case ColorRole::LexerAdded:              return {qRgb(0x09,0x81,0x0a), qRgb(0xb5,0xce,0xa8)};
    case ColorRole::LexerRemoved:            return {qRgb(0xa3,0x15,0x15), qRgb(0xf4,0x87,0x71)};
    case ColorRole::LexerChanged:            return {qRgb(0x79,0x50,0x0d), qRgb(0xd7,0xba,0x7d)};
    case ColorRole::LexerError:              return {qRgb(0xcc,0x00,0x00), qRgb(0xf4,0x47,0x47)};

    case ColorRole::ScrollBarSearchResult:   return {qRgba(255,200,0,100),   qRgba(255,200,0,150)};
    case ColorRole::ScrollBarBookmark:       return {qRgba(100,100,255,180), qRgba(100,149,237,200)};
    case ColorRole::ScrollBarCurrentLine:    return {qRgba(0,0,0,100),       qRgba(255,255,255,80)};
    case ColorRole::ScrollBarSelection:      return {qRgba(0,0,0,25),        qRgba(255,255,255,20)};

    case ColorRole::FindReplaceHighlight:    return {qRgb(0xF5,0x9E,0x0B), qRgb(0xF5,0x9E,0x0B)};

    case ColorRole::SplitterHandleBg:        return {qRgb(0xd0,0xd4,0xdc), qRgb(0x3a,0x3a,0x3a)};
    case ColorRole::SplitterHandleBorder:    return {qRgb(0xa8,0xaf,0xba), qRgb(0x5a,0x5a,0x5a)};

    case ColorRole::TopBarBackground:        return {qRgb(0xf3,0xf3,0xf3), qRgb(0x2b,0x2b,0x2b)};
    case ColorRole::TopBarBorder:            return {qRgb(0xd6,0xd6,0xd6), qRgb(0x45,0x45,0x45)};
    case ColorRole::TopBarText:              return {qRgb(0x5f,0x5f,0x5f), qRgb(0xd0,0xd0,0xd0)};
    case ColorRole::TopBarMutedText:         return {qRgb(0x78,0x78,0x78), qRgb(0x9a,0x9a,0x9a)};
    case ColorRole::TopBarHoverBg:           return {qRgb(0xe5,0xe5,0xe5), qRgb(0x3a,0x3a,0x3a)};
    case ColorRole::TopBarPressedBg:         return {qRgb(0xdb,0xdb,0xdb), qRgb(0x46,0x46,0x46)};
    case ColorRole::TopBarPopupBg:           return {qRgb(0xff,0xff,0xff), qRgb(0x2f,0x2f,0x2f)};
    case ColorRole::TopBarSelectionBg:       return {qRgb(0xd9,0xe8,0xff), qRgb(0x09,0x47,0x7b)};
    case ColorRole::TopBarSelectionText:     return {qRgb(0x1f,0x1f,0x1f), qRgb(0xff,0xff,0xff)};

    case ColorRole::FileTreeBackground:      return {qRgb(0xfb,0xfb,0xfb), qRgb(0x18,0x18,0x18)};
    case ColorRole::FileTreeText:            return {qRgb(0x1f,0x23,0x28), qRgb(0xee,0xee,0xee)};
    case ColorRole::FileTreeSelectionBg:     return {qRgb(0xd9,0xe8,0xff), qRgb(0x2f,0x5d,0x8c)};
    case ColorRole::FileTreeSelectionText:   return {qRgb(0x1f,0x23,0x28), qRgb(0xff,0xff,0xff)};
    case ColorRole::FileTreeHoverBg:         return {qRgb(0xef,0xf4,0xff), qRgb(0x2a,0x2a,0x2a)};
    case ColorRole::FileTreeBorder:          return {qRgb(0xd6,0xd6,0xd6), qRgb(0x3a,0x3a,0x3a)};

    case ColorRole::SwitchPopupBorder:       return {qRgb(0xC0,0xC0,0xC0), qRgb(0x55,0x55,0x55)};
    case ColorRole::SwitchPopupBackground:   return {qRgb(0xff,0xff,0xff), qRgb(0x2d,0x2d,0x2d)};
    case ColorRole::SwitchPopupSelectedItemBg: return {qRgb(0x31,0x6A,0xC5), qRgb(0x09,0x47,0x7b)};
    case ColorRole::SwitchPopupSelectedItemFg: return {qRgb(0xff,0xff,0xff), qRgb(0xff,0xff,0xff)};
    case ColorRole::SwitchPopupText:         return {qRgb(0x00,0x00,0x00), qRgb(0xd4,0xd4,0xd4)};

    case ColorRole::PrefWindowBg:            return {qRgb(0xf3,0xf3,0xf3), qRgb(0x22,0x22,0x22)};
    case ColorRole::PrefPanelBg:             return {qRgb(0xff,0xff,0xff), qRgb(0x2b,0x2b,0x2b)};
    case ColorRole::PrefInputBg:             return {qRgb(0xff,0xff,0xff), qRgb(0x24,0x24,0x24)};
    case ColorRole::PrefBorder:              return {qRgb(0xd6,0xd6,0xd6), qRgb(0x45,0x45,0x45)};
    case ColorRole::PrefText:                return {qRgb(0x1f,0x23,0x28), qRgb(0xee,0xee,0xee)};
    case ColorRole::PrefMutedText:           return {qRgb(0x62,0x68,0x72), qRgb(0xa0,0xa0,0xa0)};
    case ColorRole::PrefHoverBg:             return {qRgb(0xef,0xf4,0xff), qRgb(0x3a,0x3a,0x3a)};
    case ColorRole::PrefPressedBg:           return {qRgb(0xdb,0xdb,0xdb), qRgb(0x46,0x46,0x46)};
    case ColorRole::PrefAccent:              return {qRgb(0x31,0x6A,0xC5), qRgb(0x58,0xa6,0xff)};
    case ColorRole::PrefAccentPressed:       return {qRgb(0x25,0x56,0xa8), qRgb(0x38,0x82,0xd6)};
    case ColorRole::PrefAccentText:          return {qRgb(0xff,0xff,0xff), qRgb(0xff,0xff,0xff)};

    case ColorRole::MarkdownBodyBg:          return {qRgb(0xff,0xff,0xff), qRgb(0x1e,0x1e,0x1e)};
    case ColorRole::MarkdownBodyFg:          return {qRgb(0x1f,0x23,0x28), qRgb(0xc9,0xd1,0xd9)};
    case ColorRole::MarkdownHeadingFg:       return {qRgb(0x11,0x18,0x27), qRgb(0xf0,0xf0,0xf0)};
    case ColorRole::MarkdownHeadingBorder:   return {qRgb(0xd0,0xd7,0xde), qRgb(0x30,0x30,0x30)};
    case ColorRole::MarkdownCodeBg:          return {qRgb(0xf6,0xf8,0xfa), qRgb(0x16,0x1b,0x22)};
    case ColorRole::MarkdownCodeBorder:      return {qRgb(0xd0,0xd7,0xde), qRgb(0x30,0x30,0x30)};
    case ColorRole::MarkdownBlockquoteBorder: return {qRgb(0xd0,0xd7,0xde), qRgb(0x3b,0x3b,0x3b)};
    case ColorRole::MarkdownBlockquoteFg:    return {qRgb(0x57,0x60,0x6a), qRgb(0x8b,0x94,0x9e)};
    case ColorRole::MarkdownTableBorder:     return {qRgb(0xd0,0xd7,0xde), qRgb(0x30,0x30,0x30)};
    case ColorRole::MarkdownLinkFg:          return {qRgb(0x09,0x69,0xda), qRgb(0x58,0xa6,0xff)};
    }
    return {qRgb(0x00,0x00,0x00), qRgb(0xff,0xff,0xff)};
}

inline QColor color(ColorRole role)
{
    const RoleColors entry = roleColors(role);
    return QColor::fromRgba(isDarkMode() ? entry.dark : entry.light);
}

// ---------- editor (QScintilla) ----------

namespace Editor {
    inline QColor marginBg()           { return color(ColorRole::EditorMarginBg); }
    inline QColor marginFg()           { return color(ColorRole::EditorMarginFg); }
    inline QColor caretLineBg()        { return color(ColorRole::EditorCaretLineBg); }
    inline QColor caretFg()            { return color(ColorRole::EditorCaretFg); }
    inline QColor background()         { return color(ColorRole::EditorBackground); }
    inline QColor foreground()         { return color(ColorRole::EditorForeground); }
    inline QColor indentGuide()        { return color(ColorRole::EditorIndentGuide); }
    inline QColor foldMarker()         { return color(ColorRole::EditorFoldMarker); }
    inline QColor matchedBraceFg()     { return color(ColorRole::EditorMatchedBraceFg); }
    inline QColor matchedBraceBg()     { return color(ColorRole::EditorMatchedBraceBg); }
}

// ---------- lexer syntax highlighting ----------

namespace Lexer {
    enum class Role {
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

    inline QColor defaultPaper()       { return color(ColorRole::LexerDefaultPaper); }
    inline QColor defaultColor()       { return color(ColorRole::LexerDefaultColor); }
    inline QColor keyword()            { return color(ColorRole::LexerKeyword); }
    inline QColor string()             { return color(ColorRole::LexerString); }
    inline QColor comment()            { return color(ColorRole::LexerComment); }
    inline QColor number()             { return color(ColorRole::LexerNumber); }
    inline QColor preprocessor()       { return color(ColorRole::LexerPreprocessor); }
    inline QColor operatorColor()      { return color(ColorRole::LexerOperator); }
    inline QColor identifier()         { return color(ColorRole::LexerIdentifier); }
    inline QColor docKeyword()         { return color(ColorRole::LexerDocKeyword); }
    inline QColor type()               { return color(ColorRole::LexerType); }
    inline QColor documentation()      { return color(ColorRole::LexerDocumentation); }
    inline QColor builtin()            { return color(ColorRole::LexerBuiltin); }
    inline QColor function()           { return color(ColorRole::LexerFunction); }
    inline QColor variable()           { return color(ColorRole::LexerVariable); }
    inline QColor property()           { return color(ColorRole::LexerProperty); }
    inline QColor escape()             { return color(ColorRole::LexerEscape); }
    inline QColor regex()              { return color(ColorRole::LexerRegex); }
    inline QColor attribute()          { return color(ColorRole::LexerAttribute); }
    inline QColor tag()                { return color(ColorRole::LexerTag); }
    inline QColor link()               { return color(ColorRole::LexerLink); }
    inline QColor added()              { return color(ColorRole::LexerAdded); }
    inline QColor removed()            { return color(ColorRole::LexerRemoved); }
    inline QColor changed()            { return color(ColorRole::LexerChanged); }
    inline QColor error()              { return color(ColorRole::LexerError); }

    inline QColor color(Role role)
    {
        switch (role) {
        case Role::Comment: return comment();
        case Role::Documentation: return documentation();
        case Role::Keyword: return keyword();
        case Role::Type: return type();
        case Role::Builtin: return builtin();
        case Role::Function: return function();
        case Role::Variable: return variable();
        case Role::Property: return property();
        case Role::String: return string();
        case Role::Escape: return escape();
        case Role::Number: return number();
        case Role::Operator: return operatorColor();
        case Role::Preprocessor: return preprocessor();
        case Role::Regex: return regex();
        case Role::Attribute: return attribute();
        case Role::Tag: return tag();
        case Role::Link: return link();
        case Role::Added: return added();
        case Role::Removed: return removed();
        case Role::Changed: return changed();
        case Role::Error: return error();
        case Role::Default: return defaultColor();
        }
        return defaultColor();
    }
}

// ---------- scroll-bar decorator ----------

namespace ScrollBar {
    inline QColor searchResult()       { return color(ColorRole::ScrollBarSearchResult); }
    inline QColor bookmark()           { return color(ColorRole::ScrollBarBookmark); }
    inline QColor currentLine()        { return color(ColorRole::ScrollBarCurrentLine); }
    inline QColor selection()          { return color(ColorRole::ScrollBarSelection); }
}

// ---------- find / replace ----------

namespace FindReplace {
    inline QColor highlightIndicator() { return color(ColorRole::FindReplaceHighlight); }
}

// ---------- editor splitter ----------

namespace Splitter {
    inline QColor handleBg()           { return color(ColorRole::SplitterHandleBg); }
    inline QColor handleBorder()       { return color(ColorRole::SplitterHandleBorder); }
}

// ---------- editor pane top bar ----------

namespace EditorTopBar {
    inline QColor background()         { return color(ColorRole::TopBarBackground); }
    inline QColor border()             { return color(ColorRole::TopBarBorder); }
    inline QColor text()               { return color(ColorRole::TopBarText); }
    inline QColor mutedText()          { return color(ColorRole::TopBarMutedText); }
    inline QColor hoverBg()            { return color(ColorRole::TopBarHoverBg); }
    inline QColor pressedBg()          { return color(ColorRole::TopBarPressedBg); }
    inline QColor popupBg()            { return color(ColorRole::TopBarPopupBg); }
    inline QColor selectionBg()        { return color(ColorRole::TopBarSelectionBg); }
    inline QColor selectionText()      { return color(ColorRole::TopBarSelectionText); }
}

// ---------- file tree ----------

namespace FileTree {
    inline QColor background()         { return color(ColorRole::FileTreeBackground); }
    inline QColor text()               { return color(ColorRole::FileTreeText); }
    inline QColor selectionBg()        { return color(ColorRole::FileTreeSelectionBg); }
    inline QColor selectionText()      { return color(ColorRole::FileTreeSelectionText); }
    inline QColor hoverBg()            { return color(ColorRole::FileTreeHoverBg); }
    inline QColor border()             { return color(ColorRole::FileTreeBorder); }
}

// ---------- editor-switch popup ----------

namespace SwitchPopup {
    inline QColor border()             { return color(ColorRole::SwitchPopupBorder); }
    inline QColor background()         { return color(ColorRole::SwitchPopupBackground); }
    inline QColor selectedItemBg()     { return color(ColorRole::SwitchPopupSelectedItemBg); }
    inline QColor selectedItemFg()     { return color(ColorRole::SwitchPopupSelectedItemFg); }
    inline QColor text()               { return color(ColorRole::SwitchPopupText); }
}

// ---------- preferences dialog ----------

namespace Preferences {
    inline QColor windowBg()           { return color(ColorRole::PrefWindowBg); }
    inline QColor panelBg()            { return color(ColorRole::PrefPanelBg); }
    inline QColor inputBg()            { return color(ColorRole::PrefInputBg); }
    inline QColor border()             { return color(ColorRole::PrefBorder); }
    inline QColor text()               { return color(ColorRole::PrefText); }
    inline QColor mutedText()          { return color(ColorRole::PrefMutedText); }
    inline QColor hoverBg()            { return color(ColorRole::PrefHoverBg); }
    inline QColor pressedBg()          { return color(ColorRole::PrefPressedBg); }
    inline QColor accent()             { return color(ColorRole::PrefAccent); }
    inline QColor accentPressed()      { return color(ColorRole::PrefAccentPressed); }
    inline QColor accentText()         { return color(ColorRole::PrefAccentText); }
}

// ---------- markdown preview ----------

namespace MarkdownPreview {
    inline QColor bodyBg()             { return color(ColorRole::MarkdownBodyBg); }
    inline QColor bodyFg()             { return color(ColorRole::MarkdownBodyFg); }
    inline QColor headingFg()          { return color(ColorRole::MarkdownHeadingFg); }
    inline QColor headingBorder()      { return color(ColorRole::MarkdownHeadingBorder); }
    inline QColor codeBg()             { return color(ColorRole::MarkdownCodeBg); }
    inline QColor codeBorder()         { return color(ColorRole::MarkdownCodeBorder); }
    inline QColor blockquoteBorder()   { return color(ColorRole::MarkdownBlockquoteBorder); }
    inline QColor blockquoteFg()       { return color(ColorRole::MarkdownBlockquoteFg); }
    inline QColor tableBorder()        { return color(ColorRole::MarkdownTableBorder); }
    inline QColor linkFg()             { return color(ColorRole::MarkdownLinkFg); }
}

} // namespace ThemeColors

#endif // THEMECOLORS_H
