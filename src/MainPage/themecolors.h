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

// ---------- editor (QScintilla) ----------

namespace Editor {
    inline QColor marginBg()           { return isDarkMode() ? QColor(0x2d,0x2d,0x2d) : QColor(0xf0,0xf0,0xf0); }
    inline QColor marginFg()           { return isDarkMode() ? QColor(0x85,0x85,0x85) : QColor(0x66,0x66,0x66); }
    inline QColor caretLineBg()        { return isDarkMode() ? QColor(0x2a,0x2a,0x2a) : QColor(0xe8,0xe8,0xff); }
    inline QColor caretFg()            { return isDarkMode() ? QColor(0xff,0xff,0xff) : QColor(0x00,0x00,0x00); }
    inline QColor background()         { return isDarkMode() ? QColor(0x1e,0x1e,0x1e) : QColor(0xff,0xff,0xff); }
    inline QColor foreground()         { return isDarkMode() ? QColor(0xd4,0xd4,0xd4) : QColor(0x00,0x00,0x00); }
    inline QColor indentGuide()        { return isDarkMode() ? QColor(0x40,0x40,0x40) : QColor(0xd3,0xd3,0xd3); }
    inline QColor foldMarker()         { return isDarkMode() ? QColor(0x8a,0x8a,0x8a) : QColor(0x6b,0x6b,0x6b); }
    inline QColor matchedBraceFg()     { return isDarkMode() ? QColor(0x56,0x9c,0xd6) : Qt::blue; }
    inline QColor matchedBraceBg()     { return isDarkMode() ? QColor(0x3b,0x3b,0x3b) : Qt::yellow; }
}

// ---------- lexer syntax highlighting (dark-mode overrides) ----------

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

    // Default text / paper
    inline QColor defaultPaper()       { return isDarkMode() ? QColor(0x1e,0x1e,0x1e) : QColor(0xff,0xff,0xff); }
    inline QColor defaultColor()       { return isDarkMode() ? QColor(0xd4,0xd4,0xd4) : QColor(0x00,0x00,0x00); }

    // Common syntax style colours (dark-mode friendly)
    inline QColor keyword()            { return isDarkMode() ? QColor(0x56,0x9c,0xd6) : QColor(0x00,0x00,0xff); }
    inline QColor string()             { return isDarkMode() ? QColor(0xce,0x91,0x78) : QColor(0xa3,0x15,0x15); }
    inline QColor comment()            { return isDarkMode() ? QColor(0x6a,0x99,0x55) : QColor(0x00,0x80,0x00); }
    inline QColor number()             { return isDarkMode() ? QColor(0xb5,0xce,0xa8) : QColor(0x00,0x00,0x80); }
    inline QColor preprocessor()       { return isDarkMode() ? QColor(0xc5,0x86,0xc0) : QColor(0x80,0x00,0x80); }
    inline QColor operatorColor()      { return isDarkMode() ? QColor(0xd4,0xd4,0xd4) : QColor(0x00,0x00,0x00); }
    inline QColor identifier()         { return isDarkMode() ? QColor(0xdc,0xdc,0xdc) : QColor(0x00,0x00,0x00); }
    inline QColor docKeyword()         { return isDarkMode() ? QColor(0x4e,0xc9,0xb0) : QColor(0x00,0x80,0x80); }
    inline QColor type()               { return isDarkMode() ? QColor(0x4e,0xc9,0xb0) : QColor(0x2b,0x91,0xaf); }
    inline QColor documentation()      { return isDarkMode() ? QColor(0x60,0x8b,0x4e) : QColor(0x3f,0x7f,0x5f); }
    inline QColor builtin()            { return isDarkMode() ? QColor(0xd7,0xba,0x7d) : QColor(0x79,0x50,0x0d); }
    inline QColor function()           { return isDarkMode() ? QColor(0xdc,0xdc,0xaa) : QColor(0x79,0x50,0x0d); }
    inline QColor variable()           { return isDarkMode() ? QColor(0x9c,0xdc,0xfe) : QColor(0x00,0x16,0x7c); }
    inline QColor property()           { return isDarkMode() ? QColor(0x9c,0xdc,0xfe) : QColor(0x88,0x10,0x00); }
    inline QColor escape()             { return isDarkMode() ? QColor(0xd7,0xba,0x7d) : QColor(0xee,0x00,0x00); }
    inline QColor regex()              { return isDarkMode() ? QColor(0xd1,0x69,0x69) : QColor(0x81,0x1f,0x3f); }
    inline QColor attribute()          { return isDarkMode() ? QColor(0x9c,0xdc,0xfe) : QColor(0x99,0x1f,0x66); }
    inline QColor tag()                { return isDarkMode() ? QColor(0x56,0x9c,0xd6) : QColor(0x80,0x00,0x00); }
    inline QColor link()               { return isDarkMode() ? QColor(0x4f,0xc1,0xff) : QColor(0x00,0x00,0xee); }
    inline QColor added()              { return isDarkMode() ? QColor(0xb5,0xce,0xa8) : QColor(0x09,0x81,0x0a); }
    inline QColor removed()            { return isDarkMode() ? QColor(0xf4,0x87,0x71) : QColor(0xa3,0x15,0x15); }
    inline QColor changed()            { return isDarkMode() ? QColor(0xd7,0xba,0x7d) : QColor(0x79,0x50,0x0d); }
    inline QColor error()              { return isDarkMode() ? QColor(0xf4,0x47,0x47) : QColor(0xcc,0x00,0x00); }

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
    inline QColor searchResult()       { return isDarkMode() ? QColor(255,200,0,150)   : QColor(255,200,0,100); }
    inline QColor bookmark()           { return isDarkMode() ? QColor(100,149,237,200) : QColor(100,100,255,180); }
    inline QColor currentLine()        { return isDarkMode() ? QColor(255,255,255,80)  : QColor(0,0,0,100); }
    inline QColor selection()          { return isDarkMode() ? QColor(255,255,255,20)  : QColor(0,0,0,25); }
}

// ---------- find / replace ----------

namespace FindReplace {
    inline QColor highlightIndicator() { return isDarkMode() ? QColor(0xF5,0x9E,0x0B) : QColor(0xF5,0x9E,0x0B); }
}

// ---------- editor splitter ----------

namespace Splitter {
    inline QColor handleBg()           { return isDarkMode() ? QColor(0x3a,0x3a,0x3a) : QColor(0xd0,0xd4,0xdc); }
    inline QColor handleBorder()       { return isDarkMode() ? QColor(0x5a,0x5a,0x5a) : QColor(0xa8,0xaf,0xba); }
}

// ---------- editor pane top bar ----------

namespace EditorTopBar {
    inline QColor background()         { return isDarkMode() ? QColor(0x2b,0x2b,0x2b) : QColor(0xf3,0xf3,0xf3); }
    inline QColor border()             { return isDarkMode() ? QColor(0x45,0x45,0x45) : QColor(0xd6,0xd6,0xd6); }
    inline QColor text()               { return isDarkMode() ? QColor(0xd0,0xd0,0xd0) : QColor(0x5f,0x5f,0x5f); }
    inline QColor mutedText()          { return isDarkMode() ? QColor(0x9a,0x9a,0x9a) : QColor(0x78,0x78,0x78); }
    inline QColor hoverBg()            { return isDarkMode() ? QColor(0x3a,0x3a,0x3a) : QColor(0xe5,0xe5,0xe5); }
    inline QColor pressedBg()          { return isDarkMode() ? QColor(0x46,0x46,0x46) : QColor(0xdb,0xdb,0xdb); }
    inline QColor popupBg()            { return isDarkMode() ? QColor(0x2f,0x2f,0x2f) : QColor(0xff,0xff,0xff); }
    inline QColor selectionBg()        { return isDarkMode() ? QColor(0x09,0x47,0x7b) : QColor(0xd9,0xe8,0xff); }
    inline QColor selectionText()      { return isDarkMode() ? QColor(0xff,0xff,0xff) : QColor(0x1f,0x1f,0x1f); }
}

// ---------- file tree ----------

namespace FileTree {
    inline QColor background()         { return isDarkMode() ? QColor(0x18,0x18,0x18) : QColor(0xfb,0xfb,0xfb); }
    inline QColor text()               { return isDarkMode() ? QColor(0xee,0xee,0xee) : QColor(0x1f,0x23,0x28); }
    inline QColor selectionBg()        { return isDarkMode() ? QColor(0x2f,0x5d,0x8c) : QColor(0xd9,0xe8,0xff); }
    inline QColor selectionText()      { return isDarkMode() ? QColor(0xff,0xff,0xff) : QColor(0x1f,0x23,0x28); }
    inline QColor hoverBg()            { return isDarkMode() ? QColor(0x2a,0x2a,0x2a) : QColor(0xef,0xf4,0xff); }
    inline QColor border()             { return isDarkMode() ? QColor(0x3a,0x3a,0x3a) : QColor(0xd6,0xd6,0xd6); }
}

// ---------- editor-switch popup ----------

namespace SwitchPopup {
    inline QColor border()             { return isDarkMode() ? QColor(0x55,0x55,0x55) : QColor(0xC0,0xC0,0xC0); }
    inline QColor background()         { return isDarkMode() ? QColor(0x2d,0x2d,0x2d) : QColor(0xff,0xff,0xff); }
    inline QColor selectedItemBg()     { return isDarkMode() ? QColor(0x09,0x47,0x7b) : QColor(0x31,0x6A,0xC5); }
    inline QColor selectedItemFg()     { return QColor(0xff,0xff,0xff); }
    inline QColor text()               { return isDarkMode() ? QColor(0xd4,0xd4,0xd4) : QColor(0x00,0x00,0x00); }
}

// ---------- preferences dialog ----------

namespace Preferences {
    inline QColor windowBg()           { return isDarkMode() ? QColor(0x22,0x22,0x22) : QColor(0xf3,0xf3,0xf3); }
    inline QColor panelBg()            { return isDarkMode() ? QColor(0x2b,0x2b,0x2b) : QColor(0xff,0xff,0xff); }
    inline QColor inputBg()            { return isDarkMode() ? QColor(0x24,0x24,0x24) : QColor(0xff,0xff,0xff); }
    inline QColor border()             { return isDarkMode() ? QColor(0x45,0x45,0x45) : QColor(0xd6,0xd6,0xd6); }
    inline QColor text()               { return isDarkMode() ? QColor(0xee,0xee,0xee) : QColor(0x1f,0x23,0x28); }
    inline QColor mutedText()          { return isDarkMode() ? QColor(0xa0,0xa0,0xa0) : QColor(0x62,0x68,0x72); }
    inline QColor hoverBg()            { return isDarkMode() ? QColor(0x3a,0x3a,0x3a) : QColor(0xef,0xf4,0xff); }
    inline QColor pressedBg()          { return isDarkMode() ? QColor(0x46,0x46,0x46) : QColor(0xdb,0xdb,0xdb); }
    inline QColor accent()             { return isDarkMode() ? QColor(0x58,0xa6,0xff) : QColor(0x31,0x6A,0xC5); }
    inline QColor accentPressed()      { return isDarkMode() ? QColor(0x38,0x82,0xd6) : QColor(0x25,0x56,0xa8); }
    inline QColor accentText()         { return QColor(0xff,0xff,0xff); }
}

// ---------- markdown preview ----------

namespace MarkdownPreview {
    inline QColor bodyBg()             { return isDarkMode() ? QColor(0x1e,0x1e,0x1e) : QColor(0xff,0xff,0xff); }
    inline QColor bodyFg()             { return isDarkMode() ? QColor(0xc9,0xd1,0xd9) : QColor(0x1f,0x23,0x28); }
    inline QColor headingFg()          { return isDarkMode() ? QColor(0xf0,0xf0,0xf0) : QColor(0x11,0x18,0x27); }
    inline QColor headingBorder()      { return isDarkMode() ? QColor(0x30,0x30,0x30) : QColor(0xd0,0xd7,0xde); }
    inline QColor codeBg()             { return isDarkMode() ? QColor(0x16,0x1b,0x22) : QColor(0xf6,0xf8,0xfa); }
    inline QColor codeBorder()         { return isDarkMode() ? QColor(0x30,0x30,0x30) : QColor(0xd0,0xd7,0xde); }
    inline QColor blockquoteBorder()   { return isDarkMode() ? QColor(0x3b,0x3b,0x3b) : QColor(0xd0,0xd7,0xde); }
    inline QColor blockquoteFg()       { return isDarkMode() ? QColor(0x8b,0x94,0x9e) : QColor(0x57,0x60,0x6a); }
    inline QColor tableBorder()        { return isDarkMode() ? QColor(0x30,0x30,0x30) : QColor(0xd0,0xd7,0xde); }
    inline QColor linkFg()             { return isDarkMode() ? QColor(0x58,0xa6,0xff) : QColor(0x09,0x69,0xda); }
}

} // namespace ThemeColors

#endif // THEMECOLORS_H
