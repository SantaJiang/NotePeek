#ifndef DOCKERFILELEXER_H
#define DOCKERFILELEXER_H

#include "editor/lexers/simplesyntaxlexer.h"

class DockerfileLexer final : public SimpleSyntaxLexer
{
public:
    explicit DockerfileLexer(QObject *parent = nullptr);
};

#endif // DOCKERFILELEXER_H
