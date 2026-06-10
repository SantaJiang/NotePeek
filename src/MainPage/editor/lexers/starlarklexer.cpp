#include "editor/lexers/starlarklexer.h"

StarlarkLexer::StarlarkLexer(QObject *parent)
    : QsciLexerPython(parent)
{
}

const char *StarlarkLexer::language() const
{
    return "Starlark";
}

const char *StarlarkLexer::keywords(int set) const
{
    if (set == 1)
    {
        return "and as break continue def elif else for if in lambda load not "
               "or pass return";
    }

    if (set == 2)
    {
        return "False None True all any bool dict dir enumerate fail getattr "
               "hasattr int len list max min print range repr reversed select "
               "set sorted str tuple type zip";
    }

    return QsciLexerPython::keywords(set);
}
