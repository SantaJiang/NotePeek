#ifndef IGNOREFILELEXER_H
#define IGNOREFILELEXER_H

#include "editor/lexers/simplesyntaxlexer.h"

class IgnoreFileLexer final : public SimpleSyntaxLexer
{
public:
    explicit IgnoreFileLexer(QObject *parent = nullptr);
};

#endif // IGNOREFILELEXER_H
