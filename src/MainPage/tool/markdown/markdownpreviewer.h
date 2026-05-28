#ifndef MARKDOWNPREVIEWER_H
#define MARKDOWNPREVIEWER_H

#include <QObject>
#include <QString>

class QWidget;
class QLiteHtmlWidget;

class MarkdownPreviewer : public QObject
{
    Q_OBJECT

public:
    explicit MarkdownPreviewer(QObject *parent = nullptr);

    bool isMarkdownFile(const QString &filePath) const;
    QLiteHtmlWidget *createPreviewWidget(const QString &markdown, const QString &filePath, QWidget *parent);
    void updatePreview(QLiteHtmlWidget *preview, const QString &markdown, const QString &filePath);
    void showPreview(const QString &markdown, const QString &filePath, QWidget *parent);
    bool exportToPdf(const QString &markdown, const QString &filePath, const QString &outputPath);

private:
    enum class RenderMode
    {
        Screen,
        Pdf
    };

    QString markdownToHtml(const QString &markdown) const;
    QString buildHtmlDocument(const QString &bodyHtml, RenderMode mode) const;
};

#endif // MARKDOWNPREVIEWER_H
