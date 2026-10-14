#include "platformstyle.h"

const PlatformStyle &PlatformStyle::current()
{
    static const PlatformStyle style;
    return style;
}
