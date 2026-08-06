#ifndef FILESEARCHER_H
#define FILESEARCHER_H

#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>
#include <memory>

// One match of a project-wide text search.
struct FileSearchMatch
{
    QString filePath;
    int line = 0;    // 0-based line number
    int column = 0;  // 0-based character index within the line
    int length = 0;  // match length in characters
    int previewColumn = 0; // match index within lineText
    QString lineText;
};

struct FileSearchSummary
{
    int scannedFileCount = 0;
    int matchCount = 0;
    bool truncated = false;
    bool canceled = false;
};

struct FileSearchOutcome
{
    QVector<FileSearchMatch> matches;
    FileSearchSummary summary;
};

// Project-wide plain-text search over a directory tree. The walk and the
// matching run off the UI thread; files are filtered through
// SupportedFilePolicy so binaries and unknown formats are skipped. Results are
// capped at MaximumMatches to bound memory and result-tree size.
class FileSearcher : public QObject
{
    Q_OBJECT

public:
    explicit FileSearcher(QObject *parent = nullptr);

    static constexpr int MaximumMatches = 2000;

    bool isRunning() const { return m_running; }
    void start(const QString &rootDirectory,
               const QString &pattern,
               bool caseSensitive,
               bool wholeWord);
    void cancel();

signals:
    void searchFinished(const FileSearchOutcome &outcome);

private:
    std::shared_ptr<std::atomic_bool> m_cancelRequested;
    bool m_running = false;
    quint64 m_generation = 0;
};

Q_DECLARE_METATYPE(FileSearchOutcome)

#endif // FILESEARCHER_H
