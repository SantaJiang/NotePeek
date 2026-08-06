#include "find/filesearcher.h"

#include "editor/documentformat.h"
#include "editor/performanceconstants.h"
#include "editor/supportedfilepolicy.h"

#include <QDirIterator>
#include <QFile>
#include <QFutureWatcher>
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

void collectMatchesInFile(const QString &filePath,
                          const QString &text,
                          const QString &pattern,
                          Qt::CaseSensitivity caseSensitivity,
                          bool wholeWord,
                          FileSearchOutcome *outcome)
{
    int searchFrom = 0;
    int lineScanPosition = 0;
    int currentLine = 0;
    int currentLineStart = 0;

    while (true)
    {
        const int position = text.indexOf(pattern, searchFrom, caseSensitivity);
        if (position < 0)
        {
            return;
        }
        searchFrom = position + qMax(1, int(pattern.size()));

        if (wholeWord && !isWholeWordMatch(text, position, int(pattern.size())))
        {
            continue;
        }

        for (int index = lineScanPosition; index < position; ++index)
        {
            if (text.at(index) == QLatin1Char('\n'))
            {
                ++currentLine;
                currentLineStart = index + 1;
            }
        }
        lineScanPosition = position;

        int lineEnd = text.indexOf(QLatin1Char('\n'), position);
        if (lineEnd < 0)
        {
            lineEnd = int(text.size());
        }
        if (lineEnd > currentLineStart && text.at(lineEnd - 1) == QLatin1Char('\r'))
        {
            --lineEnd;
        }

        FileSearchMatch match;
        match.filePath = filePath;
        match.line = currentLine;
        match.column = position - currentLineStart;
        match.length = int(pattern.size());
        constexpr int MaximumPreviewLength = 500;
        const int lineLength = lineEnd - currentLineStart;
        const int previewLength = qMin(lineLength, MaximumPreviewLength);
        const int visibleMatchLength = qMin(match.length, previewLength);
        const int preferredContextBefore = (previewLength - visibleMatchLength) / 2;
        const int maximumPreviewStart = qMax(0, lineLength - previewLength);
        const int previewStart = qBound(0,
                                        match.column - preferredContextBefore,
                                        maximumPreviewStart);
        match.previewColumn = match.column - previewStart;
        match.lineText = text.mid(currentLineStart + previewStart, previewLength);
        outcome->matches.append(match);

        if (++outcome->summary.matchCount >= FileSearcher::MaximumMatches)
        {
            outcome->summary.truncated = true;
            return;
        }
    }
}

FileSearchOutcome runSearch(const QString &rootDirectory,
                            const QString &pattern,
                            bool caseSensitive,
                            bool wholeWord,
                            std::shared_ptr<std::atomic_bool> cancelRequested)
{
    FileSearchOutcome outcome;
    const Qt::CaseSensitivity caseSensitivity = caseSensitive ? Qt::CaseSensitive
                                                              : Qt::CaseInsensitive;

    QDirIterator iterator(rootDirectory,
                          QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext())
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

        const QString filePath = iterator.next();
        if (iterator.fileInfo().size() > PerformanceConstants::LargeFileSize)
        {
            continue;
        }
        if (!SupportedFilePolicy::isSupportedTextFile(filePath))
        {
            continue;
        }

        QFile file(filePath);
        if (!file.open(QFile::ReadOnly))
        {
            continue;
        }
        const QByteArray bytes = file.readAll();
        file.close();

        QString text;
        DocumentFormat format;
        if (!DocumentFormatCodec::decode(bytes, &text, &format, nullptr))
        {
            continue;
        }

        ++outcome.summary.scannedFileCount;
        collectMatchesInFile(filePath, text, pattern, caseSensitivity, wholeWord, &outcome);
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
    watcher->setFuture(QtConcurrent::run([rootDirectory, pattern, caseSensitive, wholeWord, cancelRequested]() {
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
