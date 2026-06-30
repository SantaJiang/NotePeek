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
    inline QColor matchedBraceFg()     { return isDarkMode() ? QColor(0x56,0x9c,0xd6) : Qt::blue; }
    inline QColor matchedBraceBg()     { return isDarkMode() ? QColor(0x3b,0x3b,0x3b) : Qt::yellow; }
}

// ---------- lexer syntax highlighting (dark-mode overrides) ----------

namespace Lexer {
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

// ---------- editor-switch popup ----------

namespace SwitchPopup {
    inline QColor border()             { return isDarkMode() ? QColor(0x55,0x55,0x55) : QColor(0xC0,0xC0,0xC0); }
    inline QColor background()         { return isDarkMode() ? QColor(0x2d,0x2d,0x2d) : QColor(0xff,0xff,0xff); }
    inline QColor selectedItemBg()     { return isDarkMode() ? QColor(0x09,0x47,0x7b) : QColor(0x31,0x6A,0xC5); }
    inline QColor selectedItemFg()     { return QColor(0xff,0xff,0xff); }
    inline QColor text()               { return isDarkMode() ? QColor(0xd4,0xd4,0xd4) : QColor(0x00,0x00,0x00); }
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
