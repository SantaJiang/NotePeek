#ifndef CUSTOMSYNTAXLEXERS_H
#define CUSTOMSYNTAXLEXERS_H

#include <Qsci/qscilexercustom.h>
#include <Qsci/qscilexerjava.h>
#include <Qsci/qscilexerpython.h>

#include <QByteArray>
#include <QSet>

class SimpleSyntaxLexer : public QsciLexerCustom
{
public:
    enum Style {
        Default = 0,
        Comment = 1,
        Number = 4,
        Keyword = 5,
        DoubleQuotedString = 6,
        SingleQuotedString = 7,
        Directive = 9,
        Operator = 10,
        Identifier = 11,
        Builtin = 13
    };

    enum VariableMode {
        NoVariables,
        ShellVariables,
        QMakeVariables
    };

    SimpleSyntaxLexer(const char *languageName,
                      const char *lineComment,
                      const char *keywords,
                      const char *builtins,
                      const char *directives,
                      const char *operators,
                      bool caseInsensitive,
                      VariableMode variableMode,
                      bool doubleDashFlags,
                      QObject *parent = nullptr);

    const char *language() const override;
    const char *wordCharacters() const override;
    QString description(int style) const override;
    void styleText(int start, int end) override;

private:
    int variableLength(const QByteArray &text, int index) const;
    int flagLength(const QByteArray &text, int index) const;
    int quotedStringLength(const QByteArray &text, int index, char quote) const;
    int numberLength(const QByteArray &text, int index) const;
    int wordLength(const QByteArray &text, int index) const;
    int styleForWord(const QByteArray &word) const;
    QByteArray normalizedWord(const QByteArray &word) const;

    QByteArray m_languageName;
    QByteArray m_lineComment;
    QByteArray m_operators;
    QSet<QByteArray> m_keywords;
    QSet<QByteArray> m_builtins;
    QSet<QByteArray> m_directives;
    bool m_caseInsensitive;
    VariableMode m_variableMode;
    bool m_doubleDashFlags;
};

class DockerfileLexer final : public SimpleSyntaxLexer
{
public:
    explicit DockerfileLexer(QObject *parent = nullptr);
};

class QMakeLexer final : public SimpleSyntaxLexer
{
public:
    explicit QMakeLexer(QObject *parent = nullptr);
};

class IgnoreFileLexer final : public SimpleSyntaxLexer
{
public:
    explicit IgnoreFileLexer(QObject *parent = nullptr);
};

class GroovyLexer final : public QsciLexerJava
{
public:
    explicit GroovyLexer(QObject *parent = nullptr);
    const char *language() const override;
    const char *keywords(int set) const override;
};

class StarlarkLexer final : public QsciLexerPython
{
public:
    explicit StarlarkLexer(QObject *parent = nullptr);
    const char *language() const override;
    const char *keywords(int set) const override;
};

class MesonLexer final : public QsciLexerPython
{
public:
    explicit MesonLexer(QObject *parent = nullptr);
    const char *language() const override;
    const char *keywords(int set) const override;
};

#endif // CUSTOMSYNTAXLEXERS_H
