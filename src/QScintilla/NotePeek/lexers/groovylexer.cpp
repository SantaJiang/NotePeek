#include "NotePeek/lexers/groovylexer.h"

GroovyLexer::GroovyLexer(QObject *parent)
    : QsciLexerJava(parent)
{
}

const char *GroovyLexer::language() const
{
    return "Groovy";
}

const char *GroovyLexer::keywords(int set) const
{
    if (set != 1)
        return QsciLexerJava::keywords(set);

    return "abstract as assert boolean break byte case catch char class const "
           "continue def default do double else enum extends false final finally "
           "float for goto if implements import in instanceof int interface long "
           "native new null package private protected public return short static "
           "strictfp super switch synchronized this throw throws trait transient "
           "true try var void volatile while yield";
}
