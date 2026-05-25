#ifndef LEXERFACTORY_H
#define LEXERFACTORY_H

#include <Qsci/qscilexer.h>
#include <QString>
#include <QStringList>
#include <QHash>

class LexerFactory
{
public:
    enum Language {
        Unknown = 0,
        Python,
        CPP,
        Java,
        JavaScript,
        HTML,
        XML,
        QML,
        JSON,
        JSONL,
        SQL,
        CMAKE,
        CSS,
        MAKEFILE,
        MARKDOWN,
        TEX,
        SHELL,
        YAML,
        Rust,
        RUBY,
        AVS,
        BATCH,
        COFFEESCRIPT,
        CSHARP,
        D,
        DIFF,
        FORTRAN,
        FORTRAN77,
        LUA,
        MATLAB,
        PASCAL,
        PERL,
        PO,
        POSTSCRIPT,
        POV,
        PROPERTIES,
        TOML,
        SPICE,
        TCL,
        VERILOG,
        VHDL,
        IDL,
        OCTAVE
    };

    static QsciLexer* createLexer(Language lang);
    static Language languageFromExtension(const QString& ext);
    static QString languageName(Language lang);
    static QStringList supportedExtensions();
};

#endif // LEXERFACTORY_H
