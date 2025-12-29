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
#include "../QScintilla/Qsci/qscilexeryaml.h"
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
    case YAML:
        return new QsciLexerYAML();
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

LexerFactory::Language LexerFactory::languageFromExtension(const QString& ext)
{
    static QHash<QString, Language> extensionMap =
        {
            {"py", Python},
            {"cpp", CPP}, {"cc", CPP}, {"cxx", CPP}, {"h", CPP}, {"hpp", CPP}, {"hxx", CPP},
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
            {"makefile", MAKEFILE}, {"mk", MAKEFILE},
            {"md", MARKDOWN},
            {"tex", TEX},
            {"sh", SHELL}, {"bash", SHELL},
            {"yaml", YAML}, {"yml", YAML},
            {"rb", RUBY},
            {"avs", AVS},
            {"bat", BATCH}, {"cmd", BATCH},
            {"coffee", COFFEESCRIPT},
            {"cs", CSHARP},
            {"d", D},
            {"diff", DIFF}, {"patch", DIFF},
            {"f", FORTRAN}, {"for", FORTRAN}, {"f90", FORTRAN}, {"f95", FORTRAN}, {"f03", FORTRAN},
            {"f77", FORTRAN77},
            {"lua", LUA},
            {"m", MATLAB},
            {"pas", PASCAL}, {"p", PASCAL},
            {"pl", PERL}, {"pm", PERL},
            {"po", PO}, {"pot", PO},
            {"ps", POSTSCRIPT}, {"eps", POSTSCRIPT},
            {"pov", POV}, {"inc", POV},
            {"properties", PROPERTIES}, {"prop", PROPERTIES}, {"ini", PROPERTIES}, {"cfg", PROPERTIES},
            {"spice", SPICE}, {"cir", SPICE},
            {"tcl", TCL}, {"tk", TCL},
            {"v", VERILOG},
            {"vhdl", VHDL}, {"vhd", VHDL},
            {"idl", IDL},
            {"octave", OCTAVE}, {"oct", OCTAVE}
        };

    QString lowerExt = ext.toLower();
    return extensionMap.value(lowerExt, Unknown);
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
    case JSON: return "JSON";
    case SQL: return "SQL";
    case CMAKE: return "CMake";
    case CSS: return "CSS";
    case MAKEFILE: return "Makefile";
    case MARKDOWN: return "Markdown";
    case TEX: return "TeX";
    case SHELL: return "Shell";
    case YAML: return "YAML";
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
    static QStringList extensions = {
        "py",
        "cpp", "cc", "cxx", "h", "hpp", "hxx",
        "java",
        "js",
        "html", "htm",
        "xml", "ui", "qrc", "svg", "ts",
        "json",
        "sql",
        "cmake",
        "css",
        "makefile", "mk",
        "md",
        "tex",
        "sh", "bash",
        "yaml", "yml",
        "rb",
        "avs",
        "bat", "cmd",
        "coffee",
        "cs",
        "d",
        "diff", "patch",
        "f", "for", "f90", "f95", "f03",
        "f77",
        "lua",
        "m",
        "pas", "p",
        "pl", "pm",
        "po", "pot",
        "ps", "eps",
        "pov", "inc",
        "properties", "prop", "ini", "cfg",
        "spice", "cir",
        "tcl", "tk",
        "v",
        "vhdl", "vhd",
        "idl",
        "octave", "oct"
    };
    return extensions;
}
