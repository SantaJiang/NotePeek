#include "markdownpreviewer.h"
#include "themecolors.h"

#include <QByteArray>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QUuid>
#include <QUrl>

#include "qlitehtmlwidget.h"

extern "C" {
#include "../../../third_party/md4c/md4c-html.h"
}

namespace {

void appendHtmlFragment(const MD_CHAR *data, MD_SIZE size, void *userData)
{
    auto *html = static_cast<QByteArray *>(userData);
    html->append(data, static_cast<int>(size));
}

QByteArray loadDataUrl(const QUrl &url)
{
    const QString data = url.toString();
    if (!data.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive))
    {
        return {};
    }

    const int comma = data.indexOf(QLatin1Char(','));
    if (comma < 0)
    {
        return {};
    }

    const QString meta = data.left(comma);
    const QByteArray payload = data.mid(comma + 1).toUtf8();
    if (meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive))
    {
        return QByteArray::fromBase64(payload);
    }

    return QUrl::fromPercentEncoding(payload).toUtf8();
}

bool isInsideDirectory(const QString &filePath, const QString &rootPath)
{
    if (rootPath.isEmpty())
    {
        return false;
    }

    const QString canonicalFile = QFileInfo(filePath).canonicalFilePath();
    const QString canonicalRoot = QDir(rootPath).canonicalPath();
    if (canonicalFile.isEmpty() || canonicalRoot.isEmpty())
    {
        return false;
    }

    return canonicalFile == canonicalRoot
           || canonicalFile.startsWith(canonicalRoot + QLatin1Char('/'));
}

QByteArray loadPreviewResource(const QUrl &url, const QString &rootPath)
{
    if (url.scheme() == QStringLiteral("data"))
    {
        return loadDataUrl(url);
    }

    if (!url.isLocalFile())
    {
        return {};
    }

    const QString filePath = url.toLocalFile();
    if (!isInsideDirectory(filePath, rootPath))
    {
        return {};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    return file.readAll();
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
    auto *preview = createPreviewWidget(markdown, filePath, nullptr);
    preview->setAttribute(Qt::WA_DeleteOnClose);
    preview->setWindowTitle(filePath.isEmpty()
                            ? tr("Markdown Preview")
                            : tr("Markdown Preview - %1").arg(QFileInfo(filePath).fileName()));

    preview->resize(900, 700);

    if (parent)
    {
        const QPoint center = parent->geometry().center() - preview->rect().center();
        preview->move(center);
    }

    preview->show();
}

QLiteHtmlWidget *MarkdownPreviewer::createPreviewWidget(const QString &markdown, const QString &filePath, QWidget *parent)
{
    auto *preview = new QLiteHtmlWidget(parent);
    preview->setFocusPolicy(Qt::StrongFocus);
    connect(preview, &QLiteHtmlWidget::linkClicked, preview, [preview](const QUrl &url) {
        QUrl current = preview->url();
        QUrl target = url;
        current.setFragment({});
        target.setFragment({});

        if (!url.fragment().isEmpty() && (target == current || target.isEmpty()))
        {
            preview->scrollToAnchor(url.fragment());
            return;
        }

        QDesktopServices::openUrl(url);
    });
    updatePreview(preview, markdown, filePath);
    return preview;
}

void MarkdownPreviewer::updatePreview(QLiteHtmlWidget *preview, const QString &markdown, const QString &filePath)
{
    if (!preview) return;

    QString rootPath;
    if (!filePath.isEmpty())
    {
        const QFileInfo info(filePath);
        rootPath = info.absolutePath();
        preview->setUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
    }
    else
    {
        preview->setUrl(QUrl());
    }

    preview->setResourceHandler([rootPath](const QUrl &url) {
        return loadPreviewResource(url, rootPath);
    });

    preview->setHtml(buildHtmlDocument(markdownToHtml(markdown), RenderMode::Screen));

    QPalette vpPalette = preview->viewport()->palette();
    vpPalette.setColor(QPalette::Base, ThemeColors::MarkdownPreview::bodyBg());
    vpPalette.setColor(QPalette::Window, ThemeColors::MarkdownPreview::bodyBg());
    preview->viewport()->setPalette(vpPalette);
    preview->viewport()->setAutoFillBackground(true);
}

bool MarkdownPreviewer::exportToPdf(const QString &markdown, const QString &filePath, const QString &outputPath)
{
    if (outputPath.isEmpty())
    {
        return false;
    }

    QLiteHtmlWidget preview;
    QString rootPath;
    if (!filePath.isEmpty())
    {
        const QFileInfo info(filePath);
        rootPath = info.absolutePath();
        preview.setUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
    }

    preview.setResourceHandler([rootPath](const QUrl &url) {
        return loadPreviewResource(url, rootPath);
    });
    preview.setHtml(buildHtmlDocument(markdownToHtml(markdown), RenderMode::Pdf));

    const QFileInfo outputInfo(outputPath);
    const QString tempPath = outputInfo.absolutePath()
                             + QDir::separator()
                             + QStringLiteral(".notepeek-pdf-")
                             + QUuid::createUuid().toString(QUuid::WithoutBraces)
                             + QStringLiteral(".pdf");

    QPrinter printer(QPrinter::ScreenResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(tempPath);
    printer.setResolution(96);
    printer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4),
                                      QPageLayout::Portrait,
                                      QMarginsF(10.0, 10.0, 10.0, 10.0),
                                      QPageLayout::Millimeter));

    preview.print(&printer);

    const QFileInfo tempInfo(tempPath);
    if (!tempInfo.exists() || tempInfo.size() <= 0)
    {
        QFile::remove(tempPath);
        return false;
    }

    // Backup existing file before overwriting (QFile::copy works on all platforms,
    // unlike QFile::rename which cannot overwrite on Windows)
    QString backupPath;
    const bool targetExists = QFileInfo::exists(outputPath);
    if (targetExists)
    {
        backupPath = outputInfo.absolutePath()
                     + QDir::separator()
                     + QStringLiteral(".notepeek-pdf-backup-")
                     + QUuid::createUuid().toString(QUuid::WithoutBraces)
                     + QStringLiteral(".pdf");
        if (!QFile::copy(outputPath, backupPath))
        {
            QFile::remove(tempPath);
            return false;
        }
        if (!QFile::remove(outputPath))
        {
            QFile::remove(backupPath);
            QFile::remove(tempPath);
            return false;
        }
    }

    if (!QFile::rename(tempPath, outputPath))
    {
        // Restore from backup
        if (targetExists)
        {
            QFile::remove(outputPath);
            QFile::rename(backupPath, outputPath);
        }
        QFile::remove(tempPath);
        return false;
    }

    // Success - clean up backup
    if (targetExists)
    {
        QFile::remove(backupPath);
    }

    return true;
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

QString MarkdownPreviewer::buildHtmlDocument(const QString &bodyHtml, RenderMode mode) const
{
    namespace MP = ThemeColors::MarkdownPreview;

    // Build colour strings in CSS format
    auto cssColor = [](const QColor &c) -> QString {
        return c.name(QColor::HexRgb);
    };

    const bool pdfMode = mode == RenderMode::Pdf;
    const QString sBodyBg       = pdfMode ? QStringLiteral("#ffffff") : cssColor(MP::bodyBg());
    const QString sBodyFg       = pdfMode ? QStringLiteral("#1f2328") : cssColor(MP::bodyFg());
    const QString sHeadingFg    = pdfMode ? QStringLiteral("#111827") : cssColor(MP::headingFg());
    const QString sHeadingBrd   = pdfMode ? QStringLiteral("#d0d7de") : cssColor(MP::headingBorder());
    const QString sCodeBg       = pdfMode ? QStringLiteral("#f6f8fa") : cssColor(MP::codeBg());
    const QString sCodeBrd      = pdfMode ? QStringLiteral("#d0d7de") : cssColor(MP::codeBorder());
    const QString sBqBrd        = pdfMode ? QStringLiteral("#d0d7de") : cssColor(MP::blockquoteBorder());
    const QString sBqFg         = pdfMode ? QStringLiteral("#57606a") : cssColor(MP::blockquoteFg());
    const QString sTblBrd       = pdfMode ? QStringLiteral("#d0d7de") : cssColor(MP::tableBorder());
    const QString sLnkFg        = pdfMode ? QStringLiteral("#0969da") : cssColor(MP::linkFg());
    const QString sBodyMargin   = pdfMode ? QStringLiteral("0") : QStringLiteral("28px 34px");

    return QStringLiteral(R"(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<style>
body {
    color: %1;
    background: %2;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    font-size: 15px;
    line-height: 1.6;
    margin: %11;
}
h1, h2, h3, h4, h5, h6 {
    color: %3;
    line-height: 1.25;
    margin: 1.35em 0 0.55em;
}
h1, h2 {
    border-bottom: 1px solid %4;
    padding-bottom: 0.25em;
}
pre {
    background: %5;
    border: 1px solid %6;
    border-radius: 6px;
    padding: 12px;
    overflow: auto;
}
code {
    background: %5;
    border-radius: 4px;
    font-family: Menlo, Consolas, monospace;
    padding: 0.12em 0.3em;
}
pre code {
    border-radius: 0;
    padding: 0;
}
blockquote {
    border-left: 4px solid %7;
    color: %8;
    margin-left: 0;
    padding-left: 1em;
}
table {
    border-collapse: collapse;
}
th, td {
    border: 1px solid %9;
    padding: 6px 10px;
}
a {
    color: %10;
}
img {
    max-width: 100%;
}
</style>
</head>
<body>
%12
</body>
</html>)")
        .arg(sBodyFg)
        .arg(sBodyBg)
        .arg(sHeadingFg)
        .arg(sHeadingBrd)
        .arg(sCodeBg)
        .arg(sCodeBrd)
        .arg(sBqBrd)
        .arg(sBqFg)
        .arg(sTblBrd)
        .arg(sLnkFg)
        .arg(sBodyMargin)
        .arg(bodyHtml);
}
