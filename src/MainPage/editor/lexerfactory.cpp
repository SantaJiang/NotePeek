#include "editor/lexerfactory.h"
#include "editor/customsyntaxlexers.h"
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerjava.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexerqml.h>
#include <Qsci/qscilexeridl.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexersql.h>
#include <Qsci/qscilexercmake.h>
#include <Qsci/qscilexercss.h>
#include <Qsci/qscilexermakefile.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexertex.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexeryaml.h>
#include <Qsci/qscilexerrust.h>
#include <Qsci/qscilexerruby.h>
#include <Qsci/qscilexeravs.h>
#include <Qsci/qscilexerbatch.h>
#include <Qsci/qscilexercoffeescript.h>
#include <Qsci/qscilexercsharp.h>
#include <Qsci/qscilexerd.h>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qscilexerfortran.h>
#include <Qsci/qscilexerfortran77.h>
#include <Qsci/qscilexerlua.h>
#include <Qsci/qscilexermatlab.h>
#include <Qsci/qscilexeroctave.h>
#include <Qsci/qscilexerpascal.h>
#include <Qsci/qscilexerperl.h>
#include <Qsci/qscilexerpo.h>
#include <Qsci/qscilexerpostscript.h>
#include <Qsci/qscilexerpov.h>
#include <Qsci/qscilexerproperties.h>
#include <Qsci/qscilexerspice.h>
#include <Qsci/qscilexertcl.h>
#include <Qsci/qscilexerverilog.h>
#include <Qsci/qscilexervhdl.h>
#include <algorithm>
#include <QFileInfo>
#include <vector>

namespace {

struct LexerRule
{
    const char *pattern;
    LexerFactory::Language language;
    Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
};

const std::vector<LexerRule>& fileNameRules()
{
    static const std::vector<LexerRule> rules = {
        {"CMakeLists.txt", LexerFactory::CMAKE},
        {"CMakeLists.txt.in", LexerFactory::CMAKE},

        {"Makefile", LexerFactory::MAKEFILE},
        {"GNUmakefile", LexerFactory::MAKEFILE},
        {"BSDmakefile", LexerFactory::MAKEFILE},
        {"Makefile.am", LexerFactory::MAKEFILE},
        {"Makefile.in", LexerFactory::MAKEFILE},

        {".bashrc", LexerFactory::SHELL},
        {".bash_profile", LexerFactory::SHELL},
        {".bash_login", LexerFactory::SHELL},
        {".profile", LexerFactory::SHELL},
        {".zshrc", LexerFactory::SHELL},
        {".zprofile", LexerFactory::SHELL},
        {".zlogin", LexerFactory::SHELL},
        {".zshenv", LexerFactory::SHELL},
        {".kshrc", LexerFactory::SHELL},
        {"configure", LexerFactory::SHELL},
        {"PKGBUILD", LexerFactory::SHELL},
        {"APKBUILD", LexerFactory::SHELL},

        {"Gemfile", LexerFactory::RUBY},
        {"Rakefile", LexerFactory::RUBY},
        {"Guardfile", LexerFactory::RUBY},
        {"Capfile", LexerFactory::RUBY},
        {"Podfile", LexerFactory::RUBY},
        {"Vagrantfile", LexerFactory::RUBY},

        {"SConstruct", LexerFactory::Python},
        {"SConscript", LexerFactory::Python},

        {".editorconfig", LexerFactory::PROPERTIES},
        {".env", LexerFactory::PROPERTIES},
        {".gitconfig", LexerFactory::PROPERTIES},
        {".gitmodules", LexerFactory::PROPERTIES},

        {".clang-format", LexerFactory::YAML},
        {".clang-tidy", LexerFactory::YAML},

        {"Jenkinsfile", LexerFactory::GROOVY},
        {"Dockerfile", LexerFactory::DOCKERFILE},
        {"Containerfile", LexerFactory::DOCKERFILE},

        {"BUILD", LexerFactory::STARLARK, Qt::CaseSensitive},
        {"BUILD.bazel", LexerFactory::STARLARK},
        {"WORKSPACE", LexerFactory::STARLARK, Qt::CaseSensitive},
        {"WORKSPACE.bazel", LexerFactory::STARLARK},
        {"MODULE.bazel", LexerFactory::STARLARK},

        {"meson.build", LexerFactory::MESON},
        {"meson_options.txt", LexerFactory::MESON},

        {".gitignore", LexerFactory::IGNORE},
        {".dockerignore", LexerFactory::IGNORE},
        {".npmignore", LexerFactory::IGNORE},
        {".eslintignore", LexerFactory::IGNORE},
        {".prettierignore", LexerFactory::IGNORE},
        {".stylelintignore", LexerFactory::IGNORE},
        {".hgignore", LexerFactory::IGNORE},
        {".ignore", LexerFactory::IGNORE},
        {".fdignore", LexerFactory::IGNORE},
        {".rgignore", LexerFactory::IGNORE},
    };
    return rules;
}

static LexerFactory::Language languageFromRules(const QString &value, const std::vector<LexerRule> &rules)
{
    for (const LexerRule &rule : rules)
    {
        if (value.compare(QLatin1String(rule.pattern), rule.caseSensitivity) == 0)
        {
            return rule.language;
        }
    }

    return LexerFactory::Unknown;
}

} // namespace


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
    case JSONL:
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
    case TOML:
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
        return new QsciLexerIDL();
    case OCTAVE:
        return new QsciLexerOctave();
    case DOCKERFILE:
        return new DockerfileLexer();
    case GROOVY:
        return new GroovyLexer();
    case STARLARK:
        return new StarlarkLexer();
    case MESON:
        return new MesonLexer();
    case QMAKE:
        return new QMakeLexer();
    case IGNORE:
        return new IgnoreFileLexer();
    default:
        return nullptr;
    }
}

static const QHash<QString, LexerFactory::Language>& extensionMap()
{
    static const QHash<QString, LexerFactory::Language> map = {
        {"py", LexerFactory::Python},
        {"c", LexerFactory::CPP}, {"cpp", LexerFactory::CPP}, {"cc", LexerFactory::CPP}, {"cxx", LexerFactory::CPP},
        {"h", LexerFactory::CPP}, {"hpp", LexerFactory::CPP}, {"hxx", LexerFactory::CPP},
        {"hh", LexerFactory::CPP}, {"ipp", LexerFactory::CPP}, {"inl", LexerFactory::CPP},
        {"java", LexerFactory::Java},
        {"js", LexerFactory::JavaScript}, {"mjs", LexerFactory::JavaScript},
        {"cjs", LexerFactory::JavaScript}, {"jsx", LexerFactory::JavaScript},
        {"html", LexerFactory::HTML}, {"htm", LexerFactory::HTML}, {"xhtml", LexerFactory::HTML},
        {"xml", LexerFactory::XML}, {"ui", LexerFactory::XML}, {"qrc", LexerFactory::XML},
        {"svg", LexerFactory::XML}, {"ts", LexerFactory::XML}, {"xsd", LexerFactory::XML},
        {"xsl", LexerFactory::XML}, {"xslt", LexerFactory::XML}, {"plist", LexerFactory::XML},
        {"qml", LexerFactory::QML}, {"qmltypes", LexerFactory::QML},
        {"json", LexerFactory::JSON}, {"jsonl", LexerFactory::JSONL},
        {"sql", LexerFactory::SQL},
        {"cmake", LexerFactory::CMAKE},
        {"css", LexerFactory::CSS},
        {"makefile", LexerFactory::MAKEFILE}, {"mk", LexerFactory::MAKEFILE},
        {"md", LexerFactory::MARKDOWN}, {"markdown", LexerFactory::MARKDOWN},
        {"mdown", LexerFactory::MARKDOWN}, {"mkd", LexerFactory::MARKDOWN}, {"mkdn", LexerFactory::MARKDOWN},
        {"tex", LexerFactory::TEX},
        {"sh", LexerFactory::SHELL}, {"bash", LexerFactory::SHELL},
        {"zsh", LexerFactory::SHELL}, {"ksh", LexerFactory::SHELL},
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
        {"f90", LexerFactory::FORTRAN}, {"f95", LexerFactory::FORTRAN},
        {"f03", LexerFactory::FORTRAN}, {"f08", LexerFactory::FORTRAN},
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
        {"conf", LexerFactory::PROPERTIES}, {"desktop", LexerFactory::PROPERTIES},
        {"service", LexerFactory::PROPERTIES}, {"timer", LexerFactory::PROPERTIES},
        {"socket", LexerFactory::PROPERTIES}, {"mount", LexerFactory::PROPERTIES},
        {"path", LexerFactory::PROPERTIES}, {"target", LexerFactory::PROPERTIES},
        {"env", LexerFactory::PROPERTIES},
        {"toml", LexerFactory::TOML},
        {"spice", LexerFactory::SPICE}, {"cir", LexerFactory::SPICE},
        {"tcl", LexerFactory::TCL}, {"tk", LexerFactory::TCL},
        {"v", LexerFactory::VERILOG},
        {"vhdl", LexerFactory::VHDL}, {"vhd", LexerFactory::VHDL},
        {"idl", LexerFactory::IDL},
        {"octave", LexerFactory::OCTAVE}, {"oct", LexerFactory::OCTAVE},
        {"dockerfile", LexerFactory::DOCKERFILE}, {"containerfile", LexerFactory::DOCKERFILE},
        {"groovy", LexerFactory::GROOVY}, {"gvy", LexerFactory::GROOVY}, {"gradle", LexerFactory::GROOVY},
        {"bzl", LexerFactory::STARLARK}, {"star", LexerFactory::STARLARK},
        {"meson", LexerFactory::MESON},
        {"pro", LexerFactory::QMAKE}, {"pri", LexerFactory::QMAKE}, {"prf", LexerFactory::QMAKE},
    };
    return map;
}

static const QHash<QString, LexerFactory::Language>& compoundExtensionMap()
{
    static const QHash<QString, LexerFactory::Language> map = {
        {"cmake.in", LexerFactory::CMAKE},
    };
    return map;
}

LexerFactory::Language LexerFactory::languageFromExtension(const QString& ext)
{
    const QString normalized = ext.toLower();
    const Language lang = extensionMap().value(normalized, Unknown);
    if (lang != Unknown)
    {
        return lang;
    }

    return compoundExtensionMap().value(normalized, Unknown);
}

LexerFactory::Language LexerFactory::languageFromFileName(const QString& fileName)
{
    const QString baseName = QFileInfo(fileName).fileName();
    if (baseName.compare(QStringLiteral(".env"), Qt::CaseInsensitive) == 0
        || baseName.startsWith(QStringLiteral(".env."), Qt::CaseInsensitive))
    {
        return PROPERTIES;
    }

    if (baseName.compare(QStringLiteral("Dockerfile"), Qt::CaseInsensitive) == 0
        || baseName.startsWith(QStringLiteral("Dockerfile."), Qt::CaseInsensitive)
        || baseName.compare(QStringLiteral("Containerfile"), Qt::CaseInsensitive) == 0
        || baseName.startsWith(QStringLiteral("Containerfile."), Qt::CaseInsensitive))
    {
        return DOCKERFILE;
    }

    if (baseName.compare(QStringLiteral("Jenkinsfile"), Qt::CaseInsensitive) == 0
        || baseName.startsWith(QStringLiteral("Jenkinsfile."), Qt::CaseInsensitive))
    {
        return GROOVY;
    }

    return languageFromRules(baseName, fileNameRules());
}

LexerFactory::Language LexerFactory::languageFromFilePath(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);

    Language lang = languageFromFileName(fileInfo.fileName());
    if (lang != Unknown)
    {
        return lang;
    }

    lang = languageFromExtension(fileInfo.completeSuffix());
    if (lang != Unknown)
    {
        return lang;
    }

    return languageFromExtension(fileInfo.suffix());
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
    case JSONL: return "JSON Lines";
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
    case TOML: return "TOML";
    case SPICE: return "Spice";
    case TCL: return "TCL";
    case VERILOG: return "Verilog";
    case VHDL: return "VHDL";
    case IDL: return "IDL";
    case OCTAVE: return "Octave";
    case DOCKERFILE: return "Dockerfile";
    case GROOVY: return "Groovy";
    case STARLARK: return "Starlark";
    case MESON: return "Meson";
    case QMAKE: return "qmake";
    case IGNORE: return "Ignore file";
    default: return "Unknown";
    }
}

QStringList LexerFactory::supportedExtensions()
{
    QStringList extensions = extensionMap().keys();
    extensions.append(compoundExtensionMap().keys());
    std::sort(extensions.begin(), extensions.end());
    return extensions;
}

QStringList LexerFactory::supportedFileNames()
{
    QStringList fileNames;
    for (const LexerRule &rule : fileNameRules())
    {
        fileNames.append(QString::fromLatin1(rule.pattern));
    }

    std::sort(fileNames.begin(), fileNames.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });
    return fileNames;
}
