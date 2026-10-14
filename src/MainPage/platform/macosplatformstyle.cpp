#include "platformstyle.h"

namespace {

// This platform currently uses the shared Fusion chrome and preferences.
class MacOSPlatformStyle final : public PlatformStyle
{
};

} // namespace

const PlatformStyle &PlatformStyle::current()
{
    static const MacOSPlatformStyle style;
    return style;
}
