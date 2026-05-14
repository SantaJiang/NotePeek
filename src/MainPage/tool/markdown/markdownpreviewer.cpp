#include "markdownpreviewer.h"

#include <QByteArray>
#include <QFileInfo>
#include <QUrl>
#include <QTextBrowser>
#include <QTextDocument>

extern "C" {
#include "../../../third_party/md4c/md4c-html.h"
}

namespace {

void appendHtmlFragment(const MD_CHAR *data, MD_SIZE size, void *userData)
{
    auto *html = static_cast<QByteArray *>(userData);
    html->append(data, static_cast<int>(size));
}

} // namespace

MarkdownPreviewer::MarkdownPreviewer(QObject *parent)
    : QObject(parent)
{
}

bool MarkdownPreviewer::isMarkdownFile(const QString &filePath) const
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral("md")
           || suffix == QStringLiteral("markdown")
           || suffix == QStringLiteral("mdown")
           || suffix == QStringLiteral("mkd")
           || suffix == QStringLiteral("mkdn");
}

void MarkdownPreviewer::showPreview(const QString &markdown, const QString &filePath, QWidget *parent)
{
    auto *browser = new QTextBrowser;
    browser->setAttribute(Qt::WA_DeleteOnClose);
    browser->setReadOnly(true);
    browser->setOpenExternalLinks(true);
    browser->setWindowTitle(filePath.isEmpty()
                            ? tr("Markdown Preview")
                            : tr("Markdown Preview - %1").arg(QFileInfo(filePath).fileName()));

    if (!filePath.isEmpty())
    {
        browser->document()->setBaseUrl(QUrl::fromLocalFile(QFileInfo(filePath).absolutePath() + QLatin1Char('/')));
    }

    browser->setHtml(buildHtmlDocument(markdownToHtml(markdown)));
    browser->resize(900, 700);

    if (parent)
    {
        const QPoint center = parent->geometry().center() - browser->rect().center();
        browser->move(center);
    }

    browser->show();
}

QString MarkdownPreviewer::markdownToHtml(const QString &markdown) const
{
    const QByteArray input = markdown.toUtf8();
    QByteArray html;

    const int result = md_html(input.constData(),
                               static_cast<MD_SIZE>(input.size()),
                               appendHtmlFragment,
                               &html,
                               MD_DIALECT_GITHUB,
                               MD_HTML_FLAG_SKIP_UTF8_BOM);
    if (result != 0)
    {
        return QStringLiteral("<p>Failed to render Markdown.</p>");
    }

    return QString::fromUtf8(html);
}

QString MarkdownPreviewer::buildHtmlDocument(const QString &bodyHtml) const
{
    return QStringLiteral(R"(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<style>
body {
    color: #1f2328;
    background: #ffffff;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    font-size: 15px;
    line-height: 1.6;
    margin: 28px 34px;
}
h1, h2, h3, h4, h5, h6 {
    color: #111827;
    line-height: 1.25;
    margin: 1.35em 0 0.55em;
}
h1, h2 {
    border-bottom: 1px solid #d0d7de;
    padding-bottom: 0.25em;
}
pre {
    background: #f6f8fa;
    border: 1px solid #d0d7de;
    border-radius: 6px;
    padding: 12px;
    overflow: auto;
}
code {
    background: #f6f8fa;
    border-radius: 4px;
    font-family: Menlo, Consolas, monospace;
    padding: 0.12em 0.3em;
}
pre code {
    border-radius: 0;
    padding: 0;
}
blockquote {
    border-left: 4px solid #d0d7de;
    color: #57606a;
    margin-left: 0;
    padding-left: 1em;
}
table {
    border-collapse: collapse;
}
th, td {
    border: 1px solid #d0d7de;
    padding: 6px 10px;
}
a {
    color: #0969da;
}
img {
    max-width: 100%;
}
</style>
</head>
<body>
%1
</body>
</html>)").arg(bodyHtml);
}
