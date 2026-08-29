#ifndef NOTEPEEK_FOUNDATION_PERFORMANCECONSTANTS_H
#define NOTEPEEK_FOUNDATION_PERFORMANCECONSTANTS_H

#include <QByteArray>
#include <QtGlobal>

namespace PerformanceConstants {

constexpr qint64 LargeFileSize = 10 * 1024 * 1024;
constexpr qint64 MaximumEditableFileSize = 256LL * 1024 * 1024;
// Scintilla's layout, width tracking and lexers can all become disproportionately
// expensive on minified/generated content even when the whole file is small.
constexpr qsizetype AbnormallyLongLineBytes = 256 * 1024;
constexpr int MarkdownPreviewDelayMs = 180;
constexpr int MarkdownLargePreviewLimit = 2 * 1024 * 1024;
constexpr int WatcherDirectoryDebounceMs = 250;
constexpr int SupportCacheLimit = 4096;

inline bool containsAbnormallyLongLine(const QByteArray &content)
{
    qsizetype lineStart = 0;
    for (qsizetype index = 0; index < content.size(); ++index)
    {
        if (content.at(index) != '\n' && content.at(index) != '\r')
            continue;
        if (index - lineStart > AbnormallyLongLineBytes)
            return true;
        lineStart = index + 1;
    }
    return content.size() - lineStart > AbnormallyLongLineBytes;
}

} // namespace PerformanceConstants

#endif // NOTEPEEK_FOUNDATION_PERFORMANCECONSTANTS_H
