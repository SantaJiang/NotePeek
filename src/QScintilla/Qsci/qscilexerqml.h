// This defines the interface to the QsciLexerQML class.

#ifndef QSCILEXERQML_H
#define QSCILEXERQML_H

#include <QObject>

#include <Qsci/qsciglobal.h>
#include <Qsci/qscilexerjavascript.h>


//! \brief The QsciLexerQML class encapsulates QML syntax highlighting.
//!
//! QML uses JavaScript expressions with declarative Qt Quick object syntax, so
//! this lexer reuses Scintilla's JavaScript/C++ lexer and supplies QML-specific
//! keyword sets.
class QSCINTILLA_EXPORT QsciLexerQML : public QsciLexerJavaScript
{
    Q_OBJECT

public:
    //! Construct a QsciLexerQML with parent \a parent.  \a parent is typically
    //! the QsciScintilla instance.
    QsciLexerQML(QObject *parent = 0);

    //! Destroys the QsciLexerQML instance.
    virtual ~QsciLexerQML();

    //! Returns the name of the language.
    const char *language() const;

    //! Returns the set of keywords for the keyword set \a set recognised by the
    //! lexer as a space separated string.
    const char *keywords(int set) const;

    //! Returns the descriptive name for style number \a style.  If the style is
    //! invalid for this language then an empty QString is returned.
    QString description(int style) const;

private:
    QsciLexerQML(const QsciLexerQML &);
    QsciLexerQML &operator=(const QsciLexerQML &);
};

#endif
