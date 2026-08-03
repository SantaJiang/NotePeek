#ifndef STARLARKLEXER_H
#define STARLARKLEXER_H

#include <Qsci/qscilexerpython.h>

class StarlarkLexer final : public QsciLexerPython
{
public:
    explicit StarlarkLexer(QObject *parent = nullptr);
    const char *language() const override;
    const char *keywords(int set) const override;
};

#endif // STARLARKLEXER_H
