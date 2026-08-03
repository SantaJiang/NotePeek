#ifndef MESONLEXER_H
#define MESONLEXER_H

#include <Qsci/qscilexerpython.h>

class MesonLexer final : public QsciLexerPython
{
public:
    explicit MesonLexer(QObject *parent = nullptr);
    const char *language() const override;
    const char *keywords(int set) const override;
};

#endif // MESONLEXER_H
