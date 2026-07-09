#ifndef MARKDOWNPREVIEWER_H
#define MARKDOWNPREVIEWER_H

#include <QObject>
#include <QString>
#include <QVector>

class QWidget;
class QLiteHtmlWidget;

struct MarkdownRenderResult
{
    quint64 requestId = 0;
    QString filePath;
    QString htmlDocument;
    QVector<int> sourceLines;
};

class MarkdownPreviewer : public QObject
{
    Q_OBJECT

public:
    explicit MarkdownPreviewer(QObject *parent = nullptr);

    bool isMarkdownFile(const QString &filePath) const;
    QLiteHtmlWidget *createPreviewWidget(const QString &markdown, const QString &filePath, QWidget *parent);
    void updatePreview(QLiteHtmlWidget *preview, const QString &markdown, const QString &filePath);
    void applyRenderedPreview(QLiteHtmlWidget *preview, const MarkdownRenderResult &result);
    // Returns the globally-unique request id assigned to this render. Callers
    // must match it against the id carried by previewRendered() so that stale
    // results, and results belonging to other pages sharing this previewer, are
    // discarded.
    quint64 renderPreviewAsync(const QString &markdown, const QString &filePath);
    void showPreview(const QString &markdown, const QString &filePath, QWidget *parent);
    bool exportToPdf(const QString &markdown, const QString &filePath, const QString &outputPath);

signals:
    void previewRendered(const MarkdownRenderResult &result);

private:
    enum class RenderMode
    {
        Screen,
        Pdf
    };

    MarkdownRenderResult renderMarkdownDocument(quint64 requestId, const QString &markdown, const QString &filePath, RenderMode mode) const;
    QString buildHtmlDocument(const QString &bodyHtml, RenderMode mode) const;

    // Request ids are handed out from this single shared previewer so they are
    // unique across every page/pane; per-page counters would collide.
    quint64 m_nextRequestId = 1;
};

#endif // MARKDOWNPREVIEWER_H
