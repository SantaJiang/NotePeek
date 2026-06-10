#include "editor/lexers/ignorefilelexer.h"

IgnoreFileLexer::IgnoreFileLexer(QObject *parent)
    : SimpleSyntaxLexer("Ignore file",
                        "#",
                        "",
                        "",
                        "",
                        "!*?[]{}()/\\",
                        false,
                        NoVariables,
                        false,
                        parent)
{
}
