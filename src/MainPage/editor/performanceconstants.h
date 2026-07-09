#ifndef PERFORMANCECONSTANTS_H
#define PERFORMANCECONSTANTS_H

#include <QtGlobal>

namespace PerformanceConstants {

constexpr qint64 LargeFileSize = 10 * 1024 * 1024;
constexpr int MarkdownPreviewDelayMs = 220;
constexpr int MarkdownLargePreviewLimit = 2 * 1024 * 1024;
constexpr int WatcherDirectoryDebounceMs = 250;
constexpr int SupportCacheLimit = 4096;

} // namespace PerformanceConstants

#endif // PERFORMANCECONSTANTS_H
