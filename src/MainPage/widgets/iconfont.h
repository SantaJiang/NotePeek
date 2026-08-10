#ifndef ICONFONT_H
#define ICONFONT_H

#include <QString>

namespace IconFont {

// Registers the bundled icon font once and returns its family. An empty value
// means the resource could not be loaded, matching the callers' old fallback.
const QString &fontAwesomeFamily();

} // namespace IconFont

#endif // ICONFONT_H
