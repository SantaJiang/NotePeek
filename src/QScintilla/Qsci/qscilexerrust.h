// This defines the interface to the QsciLexerRust class.

#ifndef QSCILEXERRUST_H
#define QSCILEXERRUST_H

#include <QObject>

#include <Qsci/qsciglobal.h>
#include <Qsci/qscilexer.h>


//! \brief The QsciLexerRust class encapsulates the Scintilla Rust lexer.
class QSCINTILLA_EXPORT QsciLexerRust : public QsciLexer
{
    Q_OBJECT

public:
    //! This enum defines the meanings of the different styles used by the Rust
    //! lexer.
    enum {
        //! The default.
        Default = 0,

        //! A block comment.
        CommentBlock = 1,

        //! A line comment.
        CommentLine = 2,

        //! A documentation block comment.
        CommentBlockDoc = 3,

        //! A documentation line comment.
        CommentLineDoc = 4,

        //! A number.
        Number = 5,

        //! A primary keyword.
        Keyword = 6,

        //! A built-in type.
        BuiltInType = 7,

        //! A secondary keyword or identifier.
        KeywordSet3 = 8,

        //! A keyword from set 4.
        KeywordSet4 = 9,

        //! A keyword from set 5.
        KeywordSet5 = 10,

        //! A keyword from set 6.
        KeywordSet6 = 11,

        //! A keyword from set 7.
        KeywordSet7 = 12,

        //! A string.
        String = 13,

        //! A raw string.
        RawString = 14,

        //! A character.
        Character = 15,

        //! An operator.
        Operator = 16,

        //! An identifier.
        Identifier = 17,

        //! A lifetime.
        Lifetime = 18,

        //! A macro.
        Macro = 19,

        //! A lexing error.
        LexingError = 20,

        //! A byte string.
        ByteString = 21,

        //! A raw byte string.
        RawByteString = 22,

        //! A byte character.
        ByteCharacter = 23
    };

    //! Construct a QsciLexerRust with parent \a parent.  \a parent is typically
    //! the QsciScintilla instance.
    QsciLexerRust(QObject *parent = 0);

    //! Destroys the QsciLexerRust instance.
    virtual ~QsciLexerRust();

    //! Returns the name of the language.
    const char *language() const;

    //! Returns the name of the lexer.
    const char *lexer() const;

    //! Returns the foreground colour of the text for style number \a style.
    QColor defaultColor(int style) const;

    //! Returns the end-of-line fill for style number \a style.
    bool defaultEolFill(int style) const;

    //! Returns the font for style number \a style.
    QFont defaultFont(int style) const;

    //! Returns the background colour of the text for style number \a style.
    QColor defaultPaper(int style) const;

    //! Returns the set of keywords for the keyword set \a set recognised by the
    //! lexer as a space separated string.
    const char *keywords(int set) const;

    //! Returns the descriptive name for style number \a style.  If the style is
    //! invalid for this language then an empty QString is returned.
    QString description(int style) const;

private:
    QsciLexerRust(const QsciLexerRust &);
    QsciLexerRust &operator=(const QsciLexerRust &);
};

#endif
