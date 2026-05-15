#ifndef MARKDOWNPREVIEWER_H
#define MARKDOWNPREVIEWER_H

#include <QObject>
#include <QString>

class QWidget;
class QTextBrowser;

class MarkdownPreviewer : public QObject
{
    Q_OBJECT

public:
    explicit MarkdownPreviewer(QObject *parent = nullptr);

    bool isMarkdownFile(const QString &filePath) const;
    QTextBrowser *createPreviewWidget(const QString &markdown, const QString &filePath, QWidget *parent);
    void updatePreview(QTextBrowser *browser, const QString &markdown, const QString &filePath);
    void showPreview(const QString &markdown, const QString &filePath, QWidget *parent);

private:
    QString markdownToHtml(const QString &markdown) const;
    QString buildHtmlDocument(const QString &bodyHtml) const;
};

#endif // MARKDOWNPREVIEWER_H
