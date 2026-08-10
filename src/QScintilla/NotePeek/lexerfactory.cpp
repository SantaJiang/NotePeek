#include "NotePeek/lexerfactory.h"
#include "NotePeek/lexers/dockerfilelexer.h"
#include "NotePeek/lexers/groovylexer.h"
#include "NotePeek/lexers/ignorefilelexer.h"
#include "NotePeek/lexers/mesonlexer.h"
#include "NotePeek/lexers/qmakelexer.h"
#include "NotePeek/lexers/starlarklexer.h"
#include <Qsci/qscilexeravs.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexerbatch.h>
#include <Qsci/qscilexercmake.h>
#include <Qsci/qscilexercoffeescript.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexercsharp.h>
#include <Qsci/qscilexercss.h>
#include <Qsci/qscilexerd.h>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qscilexerfortran.h>
#include <Qsci/qscilexerfortran77.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexeridl.h>
#include <Qsci/qscilexerjava.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerlua.h>
#include <Qsci/qscilexermakefile.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexermatlab.h>
#include <Qsci/qscilexeroctave.h>
#include <Qsci/qscilexerpascal.h>
#include <Qsci/qscilexerperl.h>
#include <Qsci/qscilexerpo.h>
#include <Qsci/qscilexerpostscript.h>
#include <Qsci/qscilexerpov.h>
#include <Qsci/qscilexerproperties.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerqml.h>
#include <Qsci/qscilexerruby.h>
#include <Qsci/qscilexerrust.h>
#include <Qsci/qscilexerspice.h>
#include <Qsci/qscilexersql.h>
#include <Qsci/qscilexertcl.h>
#include <Qsci/qscilexertex.h>
#include <Qsci/qscilexerverilog.h>
#include <Qsci/qscilexervhdl.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexeryaml.h>

#include <QFileInfo>
#include <QJsonDocument>

#include <algorithm>

namespace {

using LexerCtor = QsciLexer *(*)();

template <typename T>
QsciLexer *make()
{
    return new T;
}

QsciLexer *makeCPlusPlus()
{
    auto *lexer = new QsciLexerCPP;
    // Keep the directive itself in the preprocessor style while allowing the
    // normal C++ lexer to classify the rest of the line.  In particular,
    // quoted and angle-bracket include paths then receive the string style
    // instead of inheriting the colour of #include.
    lexer->setStylePreprocessor(true);
    return lexer;
}

// One row per language: display name, lexer constructor and the
// space-separated, lowercase extension list (compound suffixes such as
// "cmake.in" are allowed). Adding a language means adding a row here and,
// if it is recognized by file name, a row in kFileNameRules below.
struct LanguageSpec
{
    LexerFactory::Language language;
    const char *displayName;
    LexerCtor create;
    const char *extensions;
};

const LanguageSpec kLanguages[] = {
    {LexerFactory::Python, "Python", make<QsciLexerPython>, "py"},
    {LexerFactory::CPP, "C++", makeCPlusPlus, "c cpp cc cxx h hpp hxx hh ipp inl"},
    {LexerFactory::Java, "Java", make<QsciLexerJava>, "java"},
    {LexerFactory::JavaScript, "JavaScript", make<QsciLexerJavaScript>, "js mjs cjs jsx"},
    {LexerFactory::HTML, "HTML", make<QsciLexerHTML>, "html htm xhtml"},
    {LexerFactory::XML, "XML", make<QsciLexerXML>, "xml ui qrc svg ts xsd xsl xslt plist"},
    {LexerFactory::QML, "QML", make<QsciLexerQML>, "qml qmltypes"},
    {LexerFactory::JSON, "JSON", make<QsciLexerJSON>, "json"},
    {LexerFactory::JSONL, "JSON Lines", make<QsciLexerJSON>, "jsonl"},
    {LexerFactory::SQL, "SQL", make<QsciLexerSQL>, "sql"},
    {LexerFactory::CMAKE, "CMake", make<QsciLexerCMake>, "cmake cmake.in"},
    {LexerFactory::CSS, "CSS", make<QsciLexerCSS>, "css"},
    {LexerFactory::MAKEFILE, "Makefile", make<QsciLexerMakefile>, "makefile mk"},
    {LexerFactory::MARKDOWN, "Markdown", make<QsciLexerMarkdown>, "md markdown mdown mkd mkdn"},
    {LexerFactory::TEX, "TeX", make<QsciLexerTeX>, "tex"},
    {LexerFactory::SHELL, "Shell", make<QsciLexerBash>, "sh bash zsh ksh"},
    {LexerFactory::YAML, "YAML", make<QsciLexerYAML>, "yaml yml"},
    {LexerFactory::Rust, "Rust", make<QsciLexerRust>, "rs"},
    {LexerFactory::RUBY, "Ruby", make<QsciLexerRuby>, "rb"},
    {LexerFactory::AVS, "AVS", make<QsciLexerAVS>, "avs"},
    {LexerFactory::BATCH, "Batch", make<QsciLexerBatch>, "bat cmd"},
    {LexerFactory::COFFEESCRIPT, "CoffeeScript", make<QsciLexerCoffeeScript>, "coffee"},
    {LexerFactory::CSHARP, "C#", make<QsciLexerCSharp>, "cs"},
    {LexerFactory::D, "D", make<QsciLexerD>, "d"},
    {LexerFactory::DIFF, "Diff", make<QsciLexerDiff>, "diff patch"},
    {LexerFactory::FORTRAN, "Fortran", make<QsciLexerFortran>, "f for f90 f95 f03 f08"},
    {LexerFactory::FORTRAN77, "Fortran 77", make<QsciLexerFortran77>, "f77"},
    {LexerFactory::LUA, "Lua", make<QsciLexerLua>, "lua"},
    {LexerFactory::MATLAB, "Matlab", make<QsciLexerMatlab>, "m"},
    {LexerFactory::PASCAL, "Pascal", make<QsciLexerPascal>, "pas p"},
    {LexerFactory::PERL, "Perl", make<QsciLexerPerl>, "pl pm"},
    {LexerFactory::PO, "PO", make<QsciLexerPO>, "po pot"},
    {LexerFactory::POSTSCRIPT, "PostScript", make<QsciLexerPostScript>, "ps eps"},
    {LexerFactory::POV, "POV", make<QsciLexerPOV>, "pov inc"},
    {LexerFactory::PROPERTIES, "Properties", make<QsciLexerProperties>,
     "properties prop ini cfg conf desktop service timer socket mount path target env"},
    {LexerFactory::TOML, "TOML", make<QsciLexerProperties>, "toml"},
    {LexerFactory::SPICE, "Spice", make<QsciLexerSpice>, "spice cir"},
    {LexerFactory::TCL, "TCL", make<QsciLexerTCL>, "tcl tk"},
    {LexerFactory::VERILOG, "Verilog", make<QsciLexerVerilog>, "v"},
    {LexerFactory::VHDL, "VHDL", make<QsciLexerVHDL>, "vhdl vhd"},
    {LexerFactory::IDL, "IDL", make<QsciLexerIDL>, "idl"},
    {LexerFactory::OCTAVE, "Octave", make<QsciLexerOctave>, "octave oct"},
    {LexerFactory::DOCKERFILE, "Dockerfile", make<DockerfileLexer>, "dockerfile containerfile"},
    {LexerFactory::GROOVY, "Groovy", make<GroovyLexer>, "groovy gvy gradle"},
    {LexerFactory::STARLARK, "Starlark", make<StarlarkLexer>, "bzl star"},
    {LexerFactory::MESON, "Meson", make<MesonLexer>, "meson"},
    {LexerFactory::QMAKE, "qmake", make<QMakeLexer>, "pro pri prf"},
    {LexerFactory::IGNORE, "Ignore file", make<IgnoreFileLexer>, ""},
};

// File name based recognition. Exact matches the name as-is; Prefix matches
// the name itself plus any "name.suffix" variant (e.g. "Dockerfile.dev").
enum class NameMatch
{
    Exact,
    Prefix
};

struct FileNameRule
{
    const char *name;
    LexerFactory::Language language;
    NameMatch match = NameMatch::Exact;
    Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
};

const FileNameRule kFileNameRules[] = {
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
    {".env", LexerFactory::PROPERTIES, NameMatch::Prefix},
    {".gitconfig", LexerFactory::PROPERTIES},
    {".gitmodules", LexerFactory::PROPERTIES},

    {".clang-format", LexerFactory::YAML},
    {".clang-tidy", LexerFactory::YAML},

    {"Jenkinsfile", LexerFactory::GROOVY, NameMatch::Prefix},
    {"Dockerfile", LexerFactory::DOCKERFILE, NameMatch::Prefix},
    {"Containerfile", LexerFactory::DOCKERFILE, NameMatch::Prefix},

    {"BUILD", LexerFactory::STARLARK, NameMatch::Exact, Qt::CaseSensitive},
    {"BUILD.bazel", LexerFactory::STARLARK},
    {"WORKSPACE", LexerFactory::STARLARK, NameMatch::Exact, Qt::CaseSensitive},
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

const LanguageSpec *specForLanguage(LexerFactory::Language lang)
{
    static const QHash<int, const LanguageSpec *> index = [] {
        QHash<int, const LanguageSpec *> map;
        for (const LanguageSpec &spec : kLanguages)
            map.insert(spec.language, &spec);
        return map;
    }();
    return index.value(lang, nullptr);
}

const QHash<QString, LexerFactory::Language> &extensionIndex()
{
    static const QHash<QString, LexerFactory::Language> index = [] {
        QHash<QString, LexerFactory::Language> map;
        for (const LanguageSpec &spec : kLanguages)
        {
            const QStringList extensions = QString::fromLatin1(spec.extensions)
                                               .split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &extension : extensions)
                map.insert(extension, spec.language);
        }
        return map;
    }();
    return index;
}

bool matchesRule(const QString &baseName, const FileNameRule &rule)
{
    const QLatin1String name(rule.name);
    if (baseName.compare(name, rule.caseSensitivity) == 0)
        return true;

    return rule.match == NameMatch::Prefix
           && baseName.startsWith(QString(name) + QLatin1Char('.'), rule.caseSensitivity);
}

bool startsWithHtmlRootTag(const QByteArray &content)
{
    if (!content.startsWith("<html"))
        return false;
    if (content.size() == 5)
        return true;
    const char delimiter = content.at(5);
    return delimiter == '>' || delimiter == ' ' || delimiter == '\t'
           || delimiter == '\r' || delimiter == '\n';
}

bool startsWithHtmlDoctype(const QByteArray &content)
{
    constexpr auto prefix = "<!doctype html";
    constexpr qsizetype prefixLength = 14;
    if (!content.startsWith(prefix))
        return false;
    if (content.size() == prefixLength)
        return true;
    const char delimiter = content.at(prefixLength);
    return delimiter == '>' || delimiter == ' ' || delimiter == '\t'
           || delimiter == '\r' || delimiter == '\n';
}

} // namespace

QsciLexer *LexerFactory::createLexer(Language lang)
{
    const LanguageSpec *spec = specForLanguage(lang);
    return spec ? spec->create() : nullptr;
}

LexerFactory::Language LexerFactory::languageFromExtension(const QString &ext)
{
    return extensionIndex().value(ext.toLower(), Unknown);
}

LexerFactory::Language LexerFactory::languageFromFileName(const QString &fileName)
{
    const QString baseName = QFileInfo(fileName).fileName();
    for (const FileNameRule &rule : kFileNameRules)
    {
        if (matchesRule(baseName, rule))
            return rule.language;
    }
    return Unknown;
}

LexerFactory::Language LexerFactory::languageFromFilePath(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);

    Language lang = languageFromFileName(fileInfo.fileName());
    if (lang != Unknown)
        return lang;

    lang = languageFromExtension(fileInfo.completeSuffix());
    if (lang != Unknown)
        return lang;

    return languageFromExtension(fileInfo.suffix());
}

LexerFactory::Language LexerFactory::languageFromContent(const QByteArray &utf8Content)
{
    // Keep detection bounded: signatures and shebangs live at the beginning,
    // and JSON parsing is attempted only for modest documents.
    constexpr qsizetype MaximumSignatureBytes = 64 * 1024;
    constexpr qsizetype MaximumJsonDetectionBytes = 1024 * 1024;
    QByteArray prefix = utf8Content.left(MaximumSignatureBytes);
    if (prefix.startsWith("\xef\xbb\xbf"))
        prefix.remove(0, 3);

    const QByteArray trimmed = prefix.trimmed();
    if (trimmed.isEmpty())
        return Unknown;

    const int firstLineEnd = trimmed.indexOf('\n');
    const QByteArray firstLine = trimmed.left(firstLineEnd < 0 ? trimmed.size() : firstLineEnd).toLower();
    if (firstLine.startsWith("#!"))
    {
        if (firstLine.contains("python"))
            return Python;
        if (firstLine.contains("node") || firstLine.contains("deno"))
            return JavaScript;
        if (firstLine.contains("ruby"))
            return RUBY;
        if (firstLine.contains("perl"))
            return PERL;
        if (firstLine.contains("lua"))
            return LUA;
        if (firstLine.contains("bash") || firstLine.contains("zsh")
            || firstLine.contains("ksh") || firstLine.endsWith("/sh"))
        {
            return SHELL;
        }
    }

    const QByteArray lowerPrefix = trimmed.left(1024).toLower();
    if (lowerPrefix.startsWith("<?xml"))
        return XML;
    if (startsWithHtmlDoctype(lowerPrefix) || startsWithHtmlRootTag(lowerPrefix))
        return HTML;
    if (lowerPrefix.startsWith("diff --git ")
        || (lowerPrefix.startsWith("--- ") && lowerPrefix.contains("\n+++ ")))
    {
        return DIFF;
    }
    if (lowerPrefix.startsWith("cmake_minimum_required(")
        || lowerPrefix.startsWith("cmake_minimum_required ("))
    {
        return CMAKE;
    }

    if (utf8Content.size() <= MaximumJsonDetectionBytes
        && (trimmed.startsWith('{') || trimmed.startsWith('[')))
    {
        QByteArray jsonContent = utf8Content;
        if (jsonContent.startsWith("\xef\xbb\xbf"))
            jsonContent.remove(0, 3);
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(jsonContent, &error);
        if (error.error == QJsonParseError::NoError && !document.isNull())
            return JSON;
    }

    return Unknown;
}

QString LexerFactory::languageName(Language lang)
{
    const LanguageSpec *spec = specForLanguage(lang);
    return spec ? QString::fromLatin1(spec->displayName) : QStringLiteral("Unknown");
}

QStringList LexerFactory::supportedExtensions()
{
    QStringList extensions = extensionIndex().keys();
    std::sort(extensions.begin(), extensions.end());
    return extensions;
}

QStringList LexerFactory::supportedFileNames()
{
    QStringList fileNames;
    for (const FileNameRule &rule : kFileNameRules)
        fileNames.append(QString::fromLatin1(rule.name));

    std::sort(fileNames.begin(), fileNames.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });
    return fileNames;
}
