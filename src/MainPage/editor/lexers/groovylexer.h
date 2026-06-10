#ifndef GROOVYLEXER_H
#define GROOVYLEXER_H

#include <Qsci/qscilexerjava.h>

class GroovyLexer final : public QsciLexerJava
{
public:
    explicit GroovyLexer(QObject *parent = nullptr);
    const char *language() const override;
    const char *keywords(int set) const override;
};

#endif // GROOVYLEXER_H
