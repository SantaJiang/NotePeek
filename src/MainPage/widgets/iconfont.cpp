#include "widgets/iconfont.h"

#include <QFontDatabase>
#include <QStringList>

const QString &IconFont::fontAwesomeFamily()
{
    static const QString family = []() {
        const int fontId = QFontDatabase::addApplicationFont(
            QStringLiteral("://resource/fontawesome-webfont.ttf"));
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        return families.isEmpty() ? QString() : families.first();
    }();
    return family;
}
