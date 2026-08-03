#include "NotePeek/lexers/qmakelexer.h"

QMakeLexer::QMakeLexer(QObject *parent)
    : SimpleSyntaxLexer("qmake",
                        "#",
                        "absolute_path basename break cache clean_path CONFIG contains count debug defined DEFINES dirname else equals error eval exists export false files first for forfiles forfile glob greaterThan if include infile isActiveConfig isEmpty isEqual join last lessThan load log lower member message option packages parseJson prompt QT relative_path replace requires return reverse size sorted SOURCES split sprintf system touch true unique unset upper warning write_file HEADERS FORMS RESOURCES TEMPLATE TARGET",
                        "PWD OUT_PWD DESTDIR OBJECTS_DIR MOC_DIR RCC_DIR UI_DIR INCLUDEPATH LIBS QMAKE_CXXFLAGS QMAKE_LFLAGS QMAKE_PROJECT_NAME",
                        "",
                        "[]{}()=:+-*/!<>|&.,;~",
                        false,
                        QMakeVariables,
                        false,
                        parent)
{
}
