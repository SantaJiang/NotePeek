#include "find/filesearcher.h"

#include "editor/backgroundworkscheduler.h"
#include "editor/performanceconstants.h"
#include "editor/projectpathfilter.h"
#include "editor/supportedfilepolicy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStringList>
#include <QStringDecoder>
#include <QtConcurrent>

namespace {

bool isWordCharacter(QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('_');
}

bool isWholeWordMatch(const QString &text, int position, int length)
{
    if (position > 0 && isWordCharacter(text.at(position - 1)))
    {
        return false;
    }
    const int end = position + length;
    if (end < text.size() && isWordCharacter(text.at(end)))
    {
        return false;
    }
    return true;
}

void collectMatchesInLine(const QString &filePath,
                          const QString &lineText,
                          int lineNumber,
                          const QString &pattern,
                          Qt::CaseSensitivity caseSensitivity,
                          bool wholeWord,
                          FileSearchOutcome *outcome)
{
    int searchFrom = 0;
    while (true)
    {
        const int position = lineText.indexOf(pattern, searchFrom, caseSensitivity);
        if (position < 0)
            return;
        searchFrom = position + qMax(1, int(pattern.size()));

        if (wholeWord && !isWholeWordMatch(lineText, position, int(pattern.size())))
            continue;

        FileSearchMatch match;
        match.filePath = filePath;
        match.line = lineNumber;
        match.column = position;
        match.length = int(pattern.size());
        constexpr int MaximumPreviewLength = 500;
        const int lineLength = int(lineText.size());
        const int previewLength = qMin(lineLength, MaximumPreviewLength);
        const int visibleMatchLength = qMin(match.length, previewLength);
        const int preferredContextBefore = (previewLength - visibleMatchLength) / 2;
        const int maximumPreviewStart = qMax(0, lineLength - previewLength);
        const int previewStart = qBound(0,
                                        match.column - preferredContextBefore,
                                        maximumPreviewStart);
        match.previewColumn = match.column - previewStart;
        match.lineText = lineText.mid(previewStart, previewLength);
        outcome->matches.append(match);

        if (++outcome->summary.matchCount >= FileSearcher::MaximumMatches)
        {
            outcome->summary.truncated = true;
            return;
        }
    }
}

bool searchFile(const QString &filePath,
                const QString &pattern,
                Qt::CaseSensitivity caseSensitivity,
                bool wholeWord,
                FileSearchOutcome *outcome,
                const std::shared_ptr<std::atomic_bool> &cancelRequested)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return false;

    const QByteArray probe = file.peek(4096);
    QStringConverter::Encoding encoding = QStringConverter::Utf8;
    qint64 offset = 0;
    const bool utf16Little = probe.startsWith("\xff\xfe");
    const bool utf16Big = probe.startsWith("\xfe\xff");
    if (utf16Little || utf16Big)
    {
        encoding = utf16Little ? QStringConverter::Utf16LE : QStringConverter::Utf16BE;
        offset = 2;
    }
    else
    {
        if (SupportedFilePolicy::looksLikeBinaryData(probe))
            return false;
        if (probe.startsWith("\xef\xbb\xbf"))
            offset = 3;
    }
    if (!file.seek(offset))
        return false;

    const qsizetype initialMatchSize = outcome->matches.size();
    const int initialMatchCount = outcome->summary.matchCount;
    QStringDecoder decoder(encoding);
    QString pendingLine;
    int pendingScanPosition = 0;
    int lineNumber = 0;
    constexpr qint64 ReadChunkSize = 64 * 1024;

    auto consumeLines = [&](bool endOfFile) {
        int lineStart = 0;
        int scanFrom = pendingScanPosition;
        while (scanFrom < pendingLine.size())
        {
            int separator = scanFrom;
            while (separator < pendingLine.size()
                   && pendingLine.at(separator) != QLatin1Char('\n')
                   && pendingLine.at(separator) != QLatin1Char('\r'))
            {
                ++separator;
            }
            if (separator == pendingLine.size())
            {
                scanFrom = separator;
                break;
            }
            if (!endOfFile && pendingLine.at(separator) == QLatin1Char('\r')
                && separator + 1 == pendingLine.size())
            {
                scanFrom = separator;
                break;
            }

            collectMatchesInLine(filePath, pendingLine.mid(lineStart, separator - lineStart),
                                 lineNumber++, pattern, caseSensitivity, wholeWord, outcome);
            if (outcome->summary.truncated)
                return;
            scanFrom = separator + 1;
            if (pendingLine.at(separator) == QLatin1Char('\r')
                && scanFrom < pendingLine.size()
                && pendingLine.at(scanFrom) == QLatin1Char('\n'))
            {
                ++scanFrom;
            }
            lineStart = scanFrom;
        }
        if (lineStart > 0)
        {
            pendingLine.remove(0, lineStart);
            scanFrom -= lineStart;
        }
        pendingScanPosition = scanFrom;
    };

    while (!file.atEnd() && !outcome->summary.truncated)
    {
        if (cancelRequested->load(std::memory_order_relaxed))
        {
            outcome->summary.canceled = true;
            return false;
        }
        const QByteArray bytes = file.read(ReadChunkSize);
        if (bytes.isEmpty() && file.error() != QFile::NoError)
            return false;
        pendingLine += decoder.decode(bytes);
        if (decoder.hasError())
        {
            outcome->matches.resize(initialMatchSize);
            outcome->summary.matchCount = initialMatchCount;
            return false;
        }
        consumeLines(false);
    }

    pendingLine += decoder.decode(QByteArray());
    if (decoder.hasError())
    {
        outcome->matches.resize(initialMatchSize);
        outcome->summary.matchCount = initialMatchCount;
        return false;
    }
    consumeLines(true);
    if (!outcome->summary.truncated && !pendingLine.isEmpty())
        collectMatchesInLine(filePath, pendingLine, lineNumber, pattern,
                             caseSensitivity, wholeWord, outcome);
    return true;
}

FileSearchOutcome runSearch(const QString &rootDirectory,
                            const QString &pattern,
                            bool caseSensitive,
                            bool wholeWord,
                            std::shared_ptr<std::atomic_bool> cancelRequested)
{
    FileSearchOutcome outcome;
    outcome.matches.reserve(FileSearcher::MaximumMatches);
    const Qt::CaseSensitivity caseSensitivity = caseSensitive ? Qt::CaseSensitive
                                                              : Qt::CaseInsensitive;

    const ProjectPathFilter pathFilter(rootDirectory);
    QStringList pendingDirectories{QDir(rootDirectory).absolutePath()};
    while (!pendingDirectories.isEmpty())
    {
        if (cancelRequested->load())
        {
            outcome.summary.canceled = true;
            return outcome;
        }
        if (outcome.summary.truncated)
        {
            return outcome;
        }

        const QString directoryPath = pendingDirectories.takeLast();
        const QFileInfoList entries = QDir(directoryPath).entryInfoList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
            QDir::Name | QDir::DirsFirst);
        for (const QFileInfo &entry : entries)
        {
            if (cancelRequested->load())
            {
                outcome.summary.canceled = true;
                return outcome;
            }
            if (entry.isDir())
            {
                if (!entry.isSymLink()
                    && !pathFilter.ignores(entry.absoluteFilePath(), true))
                {
                    pendingDirectories.append(entry.absoluteFilePath());
                }
                continue;
            }
            if (entry.size() > PerformanceConstants::MaximumEditableFileSize
                || pathFilter.ignores(entry.absoluteFilePath(), false)
                || !SupportedFilePolicy::hasSupportedTextType(entry))
            {
                continue;
            }

            const QString filePath = entry.absoluteFilePath();
            if (searchFile(filePath, pattern, caseSensitivity, wholeWord,
                           &outcome, cancelRequested))
                ++outcome.summary.scannedFileCount;
            if (outcome.summary.canceled || outcome.summary.truncated)
                return outcome;
        }
    }

    return outcome;
}

} // namespace

FileSearcher::FileSearcher(QObject *parent)
    : QObject(parent)
{
}

void FileSearcher::start(const QString &rootDirectory,
                         const QString &pattern,
                         bool caseSensitive,
                         bool wholeWord)
{
    cancel();

    if (rootDirectory.isEmpty() || pattern.isEmpty())
    {
        return;
    }

    auto cancelRequested = std::make_shared<std::atomic_bool>(false);
    m_cancelRequested = cancelRequested;
    const quint64 generation = ++m_generation;
    m_running = true;

    auto *watcher = new QFutureWatcher<FileSearchOutcome>(this);
    connect(watcher, &QFutureWatcher<FileSearchOutcome>::finished,
            this, [this, watcher, generation]() {
        const FileSearchOutcome outcome = watcher->result();
        watcher->deleteLater();
        // A newer search may already be running; its watcher will report.
        if (generation != m_generation)
        {
            return;
        }
        m_running = false;
        emit searchFinished(outcome);
    });
    watcher->setFuture(QtConcurrent::run(BackgroundWorkScheduler::interactivePool(), [rootDirectory, pattern, caseSensitive, wholeWord, cancelRequested]() {
        return runSearch(rootDirectory, pattern, caseSensitive, wholeWord, cancelRequested);
    }));
}

void FileSearcher::cancel()
{
    if (m_cancelRequested)
    {
        m_cancelRequested->store(true);
    }
    m_running = false;
}
