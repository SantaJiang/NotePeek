#include "lexerfactory.h"
#include "../QScintilla/Qsci/qscilexerpython.h"
#include "../QScintilla/Qsci/qscilexercpp.h"
#include "../QScintilla/Qsci/qscilexerjava.h"
#include "../QScintilla/Qsci/qscilexerjavascript.h"
#include "../QScintilla/Qsci/qscilexerhtml.h"
#include "../QScintilla/Qsci/qscilexerxml.h"
#include "../QScintilla/Qsci/qscilexerqml.h"
#include "../QScintilla/Qsci/qscilexerjson.h"
#include "../QScintilla/Qsci/qscilexersql.h"
#include "../QScintilla/Qsci/qscilexercmake.h"
#include "../QScintilla/Qsci/qscilexercss.h"
#include "../QScintilla/Qsci/qscilexermakefile.h"
#include "../QScintilla/Qsci/qscilexermarkdown.h"
#include "../QScintilla/Qsci/qscilexertex.h"
#include "../QScintilla/Qsci/qscilexerbash.h"
#include "../QScintilla/Qsci/qscilexeryaml.h"
#include "../QScintilla/Qsci/qscilexerrust.h"
#include "../QScintilla/Qsci/qscilexerruby.h"
#include "../QScintilla/Qsci/qscilexeravs.h"
#include "../QScintilla/Qsci/qscilexerbatch.h"
#include "../QScintilla/Qsci/qscilexercoffeescript.h"
#include "../QScintilla/Qsci/qscilexercsharp.h"
#include "../QScintilla/Qsci/qscilexerd.h"
#include "../QScintilla/Qsci/qscilexerdiff.h"
#include "../QScintilla/Qsci/qscilexerfortran.h"
#include "../QScintilla/Qsci/qscilexerfortran77.h"
#include "../QScintilla/Qsci/qscilexerlua.h"
#include "../QScintilla/Qsci/qscilexermatlab.h"
#include "../QScintilla/Qsci/qscilexerpascal.h"
#include "../QScintilla/Qsci/qscilexerperl.h"
#include "../QScintilla/Qsci/qscilexerpo.h"
#include "../QScintilla/Qsci/qscilexerpostscript.h"
#include "../QScintilla/Qsci/qscilexerpov.h"
#include "../QScintilla/Qsci/qscilexerproperties.h"
#include "../QScintilla/Qsci/qscilexerspice.h"
#include "../QScintilla/Qsci/qscilexertcl.h"
#include "../QScintilla/Qsci/qscilexerverilog.h"
#include "../QScintilla/Qsci/qscilexervhdl.h"
#include <algorithm>

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
    case QML:
        return new QsciLexerQML();
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
    case YAML:
        return new QsciLexerYAML();
    case Rust:
        return new QsciLexerRust();
    case RUBY:
        return new QsciLexerRuby();
    case AVS:
        return new QsciLexerAVS();
    case BATCH:
        return new QsciLexerBatch();
    case COFFEESCRIPT:
        return new QsciLexerCoffeeScript();
    case CSHARP:
        return new QsciLexerCSharp();
    case D:
        return new QsciLexerD();
    case DIFF:
        return new QsciLexerDiff();
    case FORTRAN:
        return new QsciLexerFortran();
    case FORTRAN77:
        return new QsciLexerFortran77();
    case LUA:
        return new QsciLexerLua();
    case MATLAB:
        return new QsciLexerMatlab();
    case PASCAL:
        return new QsciLexerPascal();
    case PERL:
        return new QsciLexerPerl();
    case PO:
        return new QsciLexerPO();
    case POSTSCRIPT:
        return new QsciLexerPostScript();
    case POV:
        return new QsciLexerPOV();
    case PROPERTIES:
        return new QsciLexerProperties();
    case SPICE:
        return new QsciLexerSpice();
    case TCL:
        return new QsciLexerTCL();
    case VERILOG:
        return new QsciLexerVerilog();
    case VHDL:
        return new QsciLexerVHDL();
    case IDL:
        return new QsciLexerCPP();
    case OCTAVE:
        return new QsciLexerMatlab();
    default:
        return nullptr;
    }
}

static const QHash<QString, LexerFactory::Language>& extensionMap()
{
    static const QHash<QString, LexerFactory::Language> map = {
        {"py", LexerFactory::Python},
        {"cpp", LexerFactory::CPP}, {"cc", LexerFactory::CPP}, {"cxx", LexerFactory::CPP},
        {"h", LexerFactory::CPP}, {"hpp", LexerFactory::CPP}, {"hxx", LexerFactory::CPP},
        {"java", LexerFactory::Java},
        {"js", LexerFactory::JavaScript},
        {"html", LexerFactory::HTML}, {"htm", LexerFactory::HTML},
        {"xml", LexerFactory::XML}, {"ui", LexerFactory::XML}, {"qrc", LexerFactory::XML},
        {"svg", LexerFactory::XML}, {"ts", LexerFactory::XML},
        {"qml", LexerFactory::QML}, {"qmltypes", LexerFactory::QML},
        {"json", LexerFactory::JSON},
        {"sql", LexerFactory::SQL},
        {"cmake", LexerFactory::CMAKE},
        {"css", LexerFactory::CSS},
        {"makefile", LexerFactory::MAKEFILE}, {"mk", LexerFactory::MAKEFILE},
        {"md", LexerFactory::MARKDOWN},
        {"tex", LexerFactory::TEX},
        {"sh", LexerFactory::SHELL}, {"bash", LexerFactory::SHELL},
        {"yaml", LexerFactory::YAML}, {"yml", LexerFactory::YAML},
        {"rs", LexerFactory::Rust},
        {"rb", LexerFactory::RUBY},
        {"avs", LexerFactory::AVS},
        {"bat", LexerFactory::BATCH}, {"cmd", LexerFactory::BATCH},
        {"coffee", LexerFactory::COFFEESCRIPT},
        {"cs", LexerFactory::CSHARP},
        {"d", LexerFactory::D},
        {"diff", LexerFactory::DIFF}, {"patch", LexerFactory::DIFF},
        {"f", LexerFactory::FORTRAN}, {"for", LexerFactory::FORTRAN},
        {"f90", LexerFactory::FORTRAN}, {"f95", LexerFactory::FORTRAN}, {"f03", LexerFactory::FORTRAN},
        {"f77", LexerFactory::FORTRAN77},
        {"lua", LexerFactory::LUA},
        {"m", LexerFactory::MATLAB},
        {"pas", LexerFactory::PASCAL}, {"p", LexerFactory::PASCAL},
        {"pl", LexerFactory::PERL}, {"pm", LexerFactory::PERL},
        {"po", LexerFactory::PO}, {"pot", LexerFactory::PO},
        {"ps", LexerFactory::POSTSCRIPT}, {"eps", LexerFactory::POSTSCRIPT},
        {"pov", LexerFactory::POV}, {"inc", LexerFactory::POV},
        {"properties", LexerFactory::PROPERTIES}, {"prop", LexerFactory::PROPERTIES},
        {"ini", LexerFactory::PROPERTIES}, {"cfg", LexerFactory::PROPERTIES},
        {"spice", LexerFactory::SPICE}, {"cir", LexerFactory::SPICE},
        {"tcl", LexerFactory::TCL}, {"tk", LexerFactory::TCL},
        {"v", LexerFactory::VERILOG},
        {"vhdl", LexerFactory::VHDL}, {"vhd", LexerFactory::VHDL},
        {"idl", LexerFactory::IDL},
        {"octave", LexerFactory::OCTAVE}, {"oct", LexerFactory::OCTAVE},
    };
    return map;
}

LexerFactory::Language LexerFactory::languageFromExtension(const QString& ext)
{
    return extensionMap().value(ext.toLower(), Unknown);
}

QString LexerFactory::languageName(Language lang)
{
    switch(lang)
    {
    case Python: return "Python";
    case CPP: return "C++";
    case Java: return "Java";
    case JavaScript: return "JavaScript";
    case HTML: return "HTML";
    case XML: return "XML";
    case QML: return "QML";
    case JSON: return "JSON";
    case SQL: return "SQL";
    case CMAKE: return "CMake";
    case CSS: return "CSS";
    case MAKEFILE: return "Makefile";
    case MARKDOWN: return "Markdown";
    case TEX: return "TeX";
    case SHELL: return "Shell";
    case YAML: return "YAML";
    case Rust: return "Rust";
    case RUBY: return "Ruby";
    case AVS: return "AVS";
    case BATCH: return "Batch";
    case COFFEESCRIPT: return "CoffeeScript";
    case CSHARP: return "C#";
    case D: return "D";
    case DIFF: return "Diff";
    case FORTRAN: return "Fortran";
    case FORTRAN77: return "Fortran 77";
    case LUA: return "Lua";
    case MATLAB: return "Matlab";
    case PASCAL: return "Pascal";
    case PERL: return "Perl";
    case PO: return "PO";
    case POSTSCRIPT: return "PostScript";
    case POV: return "POV";
    case PROPERTIES: return "Properties";
    case SPICE: return "Spice";
    case TCL: return "TCL";
    case VERILOG: return "Verilog";
    case VHDL: return "VHDL";
    case IDL: return "IDL";
    case OCTAVE: return "Octave";
    default: return "Unknown";
    }
}

QStringList LexerFactory::supportedExtensions()
{
    QStringList extensions = extensionMap().keys();
    std::sort(extensions.begin(), extensions.end());
    return extensions;
}
