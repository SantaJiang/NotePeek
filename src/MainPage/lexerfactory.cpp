#include "lexerfactory.h"
#include "../QScintilla/Qsci/qscilexerpython.h"
#include "../QScintilla/Qsci/qscilexercpp.h"
#include "../QScintilla/Qsci/qscilexerjava.h"
#include "../QScintilla/Qsci/qscilexerjavascript.h"
#include "../QScintilla/Qsci/qscilexerhtml.h"
#include "../QScintilla/Qsci/qscilexerxml.h"
#include "../QScintilla/Qsci/qscilexerjson.h"
#include "../QScintilla/Qsci/qscilexersql.h"
#include "../QScintilla/Qsci/qscilexercmake.h"
#include "../QScintilla/Qsci/qscilexercss.h"
#include "../QScintilla/Qsci/qscilexermakefile.h"
#include "../QScintilla/Qsci/qscilexermarkdown.h"
#include "../QScintilla/Qsci/qscilexertex.h"
#include "../QScintilla/Qsci/qscilexerbash.h"

QsciLexer* LexerFactory::createLexer(Language lang)
{
    switch(lang)
    {
    case Python:
        return new QsciLexerPython();
    case CPP:
        return new QsciLexerCPP();
    case Java:
        return new QsciLexerJava();
    case JavaScript:
        return new QsciLexerJavaScript();
    case HTML:
        return new QsciLexerHTML();
    case XML:
        return new QsciLexerXML();
    case JSON:
        return new QsciLexerJSON();
    case SQL:
        return new QsciLexerSQL();
    case CMAKE:
        return new QsciLexerCMake();
    case CSS:
        return new QsciLexerCSS();
    case MAKEFILE:
        return new QsciLexerMakefile();
    case MARKDOWN:
        return new QsciLexerMarkdown();
    case TEX:
        return new QsciLexerTeX();
    case SHELL:
        return new QsciLexerBash();
    default:
        return nullptr;
    }
}

LexerFactory::Language LexerFactory::languageFromExtension(const QString& ext)
{
    static QHash<QString, Language> extensionMap =
    {
        {"py", Python},
        {"cpp", CPP}, {"cc", CPP}, {"cxx", CPP}, {"h", CPP}, {"hpp", CPP},
        {"java", Java},
        {"js", JavaScript},
        {"html", HTML}, {"htm", HTML},
        {"xml", XML},
        {"ui", XML},
        {"qrc", XML},
        {"svg", XML},
        {"ts", XML},
        {"json", JSON},
        {"sql", SQL},
        {"cmake", CMAKE},
        {"css", CSS},
        {"makefile", MAKEFILE},
        {"md", MARKDOWN},
        {"tex", TEX},
        {"sh", SHELL}
    };

    QString lowerExt = ext.toLower();
    return extensionMap.value(lowerExt, Unknown);
}
