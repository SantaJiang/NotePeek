// This module implements the QsciLexerRust class.

#include "Qsci/qscilexerrust.h"

#include <qcolor.h>
#include <qfont.h>


// The ctor.
QsciLexerRust::QsciLexerRust(QObject *parent)
    : QsciLexer(parent)
{
}


// The dtor.
QsciLexerRust::~QsciLexerRust()
{
}


// Returns the language name.
const char *QsciLexerRust::language() const
{
    return "Rust";
}


// Returns the lexer name.
const char *QsciLexerRust::lexer() const
{
    return "rust";
}


// Returns the foreground colour of the text for a style.
QColor QsciLexerRust::defaultColor(int style) const
{
    switch (style)
    {
    case Default:
        return QColor(0x80, 0x80, 0x80);

    case CommentBlock:
    case CommentLine:
        return QColor(0x00, 0x7f, 0x00);

    case CommentBlockDoc:
    case CommentLineDoc:
        return QColor(0x3f, 0x70, 0x3f);

    case Number:
        return QColor(0x00, 0x7f, 0x7f);

    case Keyword:
    case KeywordSet3:
    case KeywordSet4:
    case KeywordSet5:
    case KeywordSet6:
    case KeywordSet7:
        return QColor(0x00, 0x00, 0x7f);

    case BuiltInType:
        return QColor(0x00, 0x66, 0x99);

    case String:
    case RawString:
    case Character:
    case ByteString:
    case RawByteString:
    case ByteCharacter:
        return QColor(0x7f, 0x00, 0x7f);

    case Lifetime:
        return QColor(0x80, 0x40, 0x20);

    case Macro:
        return QColor(0x7f, 0x3f, 0x00);

    case Operator:
    case Identifier:
        return QColor(0x00, 0x00, 0x00);

    case LexingError:
        return QColor(0xff, 0xff, 0xff);
    }

    return QsciLexer::defaultColor(style);
}


// Returns the end-of-line fill for a style.
bool QsciLexerRust::defaultEolFill(int style) const
{
    if (style == LexingError)
        return true;

    return QsciLexer::defaultEolFill(style);
}


// Returns the font of the text for a style.
QFont QsciLexerRust::defaultFont(int style) const
{
    QFont f = QsciLexer::defaultFont(style);

    switch (style)
    {
    case Keyword:
    case BuiltInType:
    case KeywordSet3:
    case Macro:
        f.setBold(true);
        break;

    case CommentBlockDoc:
    case CommentLineDoc:
        f.setItalic(true);
        break;
    }

    return f;
}


// Returns the set of keywords.
const char *QsciLexerRust::keywords(int set) const
{
    if (set == 1)
        return
            "as async await break const continue crate dyn else enum extern false fn "
            "for if impl in let loop match mod move mut pub ref return self Self "
            "static struct super trait true type unsafe use where while yield "
            "abstract become box do final macro override priv try typeof unsized "
            "virtual";

    if (set == 2)
        return
            "bool char str i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize "
            "f32 f64";

    if (set == 3)
        return
            "Copy Send Sized Sync Unpin Drop Fn FnMut FnOnce ToOwned Clone Debug "
            "Default Eq Hash Ord PartialEq PartialOrd AsRef AsMut Borrow BorrowMut "
            "From Into TryFrom TryInto Iterator DoubleEndedIterator ExactSizeIterator "
            "Extend FromIterator IntoIterator Option Result Some None Ok Err Box "
            "String Vec";

    return 0;
}


// Returns the user name of a style.
QString QsciLexerRust::description(int style) const
{
    switch (style)
    {
    case Default:
        return tr("Default");

    case CommentBlock:
        return tr("Block comment");

    case CommentLine:
        return tr("Line comment");

    case CommentBlockDoc:
        return tr("Documentation block comment");

    case CommentLineDoc:
        return tr("Documentation line comment");

    case Number:
        return tr("Number");

    case Keyword:
        return tr("Keyword");

    case BuiltInType:
        return tr("Built-in type");

    case KeywordSet3:
        return tr("Secondary keyword and identifier");

    case KeywordSet4:
        return tr("Keyword set 4");

    case KeywordSet5:
        return tr("Keyword set 5");

    case KeywordSet6:
        return tr("Keyword set 6");

    case KeywordSet7:
        return tr("Keyword set 7");

    case String:
        return tr("String");

    case RawString:
        return tr("Raw string");

    case Character:
        return tr("Character");

    case Operator:
        return tr("Operator");

    case Identifier:
        return tr("Identifier");

    case Lifetime:
        return tr("Lifetime");

    case Macro:
        return tr("Macro");

    case LexingError:
        return tr("Lexing error");

    case ByteString:
        return tr("Byte string");

    case RawByteString:
        return tr("Raw byte string");

    case ByteCharacter:
        return tr("Byte character");
    }

    return QString();
}


// Returns the background colour of the text for a style.
QColor QsciLexerRust::defaultPaper(int style) const
{
    if (style == LexingError)
        return QColor(0xff, 0x00, 0x00);

    return QsciLexer::defaultPaper(style);
}
