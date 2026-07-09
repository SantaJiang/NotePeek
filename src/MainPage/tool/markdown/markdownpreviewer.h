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
    void renderPreviewAsync(quint64 requestId, const QString &markdown, const QString &filePath);
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
};

#endif // MARKDOWNPREVIEWER_H
