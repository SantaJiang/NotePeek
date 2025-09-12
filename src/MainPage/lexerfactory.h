#ifndef LEXERFACTORY_H
#define LEXERFACTORY_H

#include "../QScintilla/Qsci/qscilexer.h"
#include <QString>

class LexerFactory
{
public:
    enum Language {
        Python,
        CPP,
        Java,
        JavaScript,
        HTML,
        XML,
        JSON,
        SQL,
        CMAKE,
        CSS,
        HEX,
        MAKEFILE,
        MARKDOWN,
        TEX,
        SHELL,
        Unknown
    };

    static QsciLexer* createLexer(Language lang);
    static Language languageFromExtension(const QString& ext);
};

#endif // LEXERFACTORY_H
