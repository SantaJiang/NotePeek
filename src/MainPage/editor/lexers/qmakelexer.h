#ifndef QMAKELEXER_H
#define QMAKELEXER_H

#include "editor/lexers/simplesyntaxlexer.h"

class QMakeLexer final : public SimpleSyntaxLexer
{
public:
    explicit QMakeLexer(QObject *parent = nullptr);
};

#endif // QMAKELEXER_H
