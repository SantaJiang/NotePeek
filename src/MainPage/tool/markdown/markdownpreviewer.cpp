#include "tool/markdown/markdownpreviewer.h"
#include "foundation/backgroundworkscheduler.h"
#include "apppreferences.h"
#include "foundation/documentpath.h"
#include "themecolors.h"

#include <QByteArray>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QFutureWatcher>
#include <QUuid>
#include <QUrl>
#include <QtConcurrent>

#include <mutex>

#include <qlitehtmlwidget.h>

extern "C" {
#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm-extension_api.h>
#include <cmark-gfm.h>
}

namespace {

constexpr qsizetype MaximumPreviewResourceSize = 8 * 1024 * 1024;

enum class MarkdownRenderMode
{
    Screen,
    Pdf
};

struct MarkdownRenderStyle
{
    QString bodyBg;
    QString bodyFg;
    QString headingFg;
    QString headingBorder;
    QString codeBg;
    QString codeBorder;
    QString blockquoteBorder;
    QString blockquoteFg;
    QString tableBorder;
    QString linkFg;
    QString bodyMargin;
    QString bodyFontFamily;
    QString codeFontFamily;
};

MarkdownRenderStyle captureMarkdownRenderStyle(MarkdownRenderMode mode)
{
    namespace MP = ThemeColors::MarkdownPreview;

    auto cssColor = [](const QColor &c) -> QString {
        return c.name(QColor::HexRgb);
    };

    const bool pdfMode = mode == MarkdownRenderMode::Pdf;
    const AppPreferences &prefs = AppPreferences::instance();
    const QString emojiFallback = QStringLiteral(
        "\"Apple Color Emoji\", \"Segoe UI Emoji\", \"Noto Color Emoji\", "
        "\"Noto Emoji\", \"Twemoji Mozilla\", \"Twitter Color Emoji\", "
        "\"EmojiOne Color\", \"JoyPixels\", Symbola");

    MarkdownRenderStyle style;
    style.bodyBg = pdfMode ? QStringLiteral("#ffffff") : cssColor(MP::bodyBg());
    style.bodyFg = pdfMode ? QStringLiteral("#1f2328") : cssColor(MP::bodyFg());
    style.headingFg = pdfMode ? QStringLiteral("#111827") : cssColor(MP::headingFg());
    style.headingBorder = pdfMode ? QStringLiteral("#d0d7de") : cssColor(MP::headingBorder());
    style.codeBg = pdfMode ? QStringLiteral("#f6f8fa") : cssColor(MP::codeBg());
    style.codeBorder = pdfMode ? QStringLiteral("#d0d7de") : cssColor(MP::codeBorder());
    style.blockquoteBorder = pdfMode ? QStringLiteral("#d0d7de") : cssColor(MP::blockquoteBorder());
    style.blockquoteFg = pdfMode ? QStringLiteral("#57606a") : cssColor(MP::blockquoteFg());
    style.tableBorder = pdfMode ? QStringLiteral("#d0d7de") : cssColor(MP::tableBorder());
    style.linkFg = pdfMode ? QStringLiteral("#0969da") : cssColor(MP::linkFg());
    style.bodyMargin = pdfMode
                           ? QStringLiteral("0")
                           : QStringLiteral("28px %1px").arg(MarkdownPreviewLayout::ScreenHorizontalMargin);
    style.bodyFontFamily = QStringLiteral("\"%1\", -apple-system, BlinkMacSystemFont, \"Segoe UI\", %2, sans-serif")
                               .arg(prefs.textFontFamily(), emojiFallback);
    style.codeFontFamily = QStringLiteral("\"%1\", Menlo, Consolas, %2, monospace")
                               .arg(prefs.codeFontFamily(), emojiFallback);
    return style;
}

QString buildHtmlDocumentFromStyle(const QString &bodyHtml, const MarkdownRenderStyle &style)
{
    return QStringLiteral(R"(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<style>
body {
    color: %1;
    background: %2;
    font-family: %12;
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
    font-family: %13;
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
    width: 100%;
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
%14
</body>
</html>)")
        .arg(style.bodyFg)
        .arg(style.bodyBg)
        .arg(style.headingFg)
        .arg(style.headingBorder)
        .arg(style.codeBg)
        .arg(style.codeBorder)
        .arg(style.blockquoteBorder)
        .arg(style.blockquoteFg)
        .arg(style.tableBorder)
        .arg(style.linkFg)
        .arg(style.bodyMargin)
        .arg(style.bodyFontFamily)
        .arg(style.codeFontFamily)
        .arg(bodyHtml);
}

void attachGfmExtension(cmark_parser *parser, const char *name)
{
    cmark_syntax_extension *extension = cmark_find_syntax_extension(name);
    if (extension)
    {
        cmark_parser_attach_syntax_extension(parser, extension);
    }
}

void ensureCmarkExtensionsRegistered()
{
    static std::once_flag once;
    std::call_once(once, []() {
        cmark_gfm_core_extensions_ensure_registered();
    });
}

bool isTableCellWrapBreak(QChar character)
{
    switch (character.unicode()) {
    case '/':
    case '\\':
    case '-':
    case '_':
    case '.':
    case '?':
    case '#':
    case '=':
    case '%':
    case ':':
        return true;
    default:
        return false;
    }
}

QString tagNameFromMarkup(const QString &markup)
{
    int start = 0;
    if (markup.startsWith(QLatin1Char('/')))
        start = 1;
    else if (markup.startsWith(QLatin1Char('!')))
        return {};

    int end = start;
    while (end < markup.size()) {
        const QChar character = markup.at(end);
        if (character.isSpace() || character == QLatin1Char('/') || character == QLatin1Char('>'))
            break;
        ++end;
    }
    return markup.mid(start, end - start).toLower();
}

// litehtml will not break a table cell below its longest word, so a URL or
// path keeps the whole table wider than the preview. Insert U+200B after
// punctuation and along very long runs. The preview's text splitter treats
// that character as a wrap opportunity and does not draw it.
QString softenTableCellWrapping(const QString &html)
{
    constexpr int kBreakAfter = 8;
    constexpr int kForceBreak = 12;
    const QChar softBreak(0x200B);

    QString softened;
    softened.reserve(html.size() + html.size() / 16);

    int cellDepth = 0;
    int preDepth = 0;
    bool inTag = false;
    bool inEntity = false;
    int entityLength = 0;
    int run = 0;
    QString markup;

    auto appendText = [&](QChar character) {
        softened += character;
        if (cellDepth <= 0 || preDepth > 0) {
            run = 0;
            return;
        }
        if (character.isSpace()) {
            run = 0;
            return;
        }
        ++run;
        if ((isTableCellWrapBreak(character) && run >= kBreakAfter) || run >= kForceBreak) {
            softened += softBreak;
            run = 0;
        }
    };

    for (int index = 0; index < html.size(); ++index) {
        const QChar character = html.at(index);
        if (inTag) {
            softened += character;
            if (character == QLatin1Char('>')) {
                inTag = false;
                const bool closing = markup.startsWith(QLatin1Char('/'));
                const bool selfClosing = markup.endsWith(QLatin1Char('/'))
                                         || markup.startsWith(QLatin1Char('!'));
                const QString name = tagNameFromMarkup(markup);
                markup.clear();
                run = 0;
                if (name == QLatin1String("td") || name == QLatin1String("th")) {
                    if (closing)
                        cellDepth = qMax(0, cellDepth - 1);
                    else if (!selfClosing)
                        ++cellDepth;
                } else if (name == QLatin1String("pre")) {
                    if (closing)
                        preDepth = qMax(0, preDepth - 1);
                    else if (!selfClosing)
                        ++preDepth;
                }
            } else {
                markup += character;
            }
            continue;
        }

        if (character == QLatin1Char('<')) {
            inTag = true;
            markup.clear();
            run = 0;
            softened += character;
            continue;
        }

        if (inEntity) {
            softened += character;
            ++entityLength;
            if (character == QLatin1Char(';') || entityLength > 32) {
                inEntity = false;
                if (cellDepth > 0 && preDepth <= 0)
                    ++run;
            }
            continue;
        }

        if (character == QLatin1Char('&') && cellDepth > 0 && preDepth <= 0) {
            inEntity = true;
            entityLength = 0;
            softened += character;
            continue;
        }

        appendText(character);
    }

    return softened;
}

QString markdownToHtmlBody(const QString &markdown,
                           const std::shared_ptr<std::atomic_bool> &cancellation = {})
{
    const QByteArray input = markdown.toUtf8();

    ensureCmarkExtensionsRegistered();

    const int options = CMARK_OPT_SOURCEPOS
                        | CMARK_OPT_TABLE_PREFER_STYLE_ATTRIBUTES
                        | CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE;
    cmark_parser *parser = cmark_parser_new(options);
    if (!parser)
    {
        return QStringLiteral("<p>Failed to render Markdown.</p>");
    }

    attachGfmExtension(parser, "table");
    attachGfmExtension(parser, "tasklist");
    attachGfmExtension(parser, "strikethrough");
    attachGfmExtension(parser, "autolink");
    attachGfmExtension(parser, "tagfilter");

    cmark_parser_feed(parser, input.constData(), static_cast<size_t>(input.size()));
    cmark_node *document = cmark_parser_finish(parser);
    if (!document)
    {
        cmark_parser_free(parser);
        return QStringLiteral("<p>Failed to render Markdown.</p>");
    }

    char *html = cmark_render_html(document, options, cmark_parser_get_syntax_extensions(parser));
    QString rendered;
    if (html)
    {
        if (!cancellation || !cancellation->load(std::memory_order_relaxed))
        {
            rendered = softenTableCellWrapping(QString::fromUtf8(html));
        }
        cmark_get_default_mem_allocator()->free(html);
    }

    cmark_node_free(document);
    cmark_parser_free(parser);

    return rendered.isEmpty() ? QStringLiteral("<p>Failed to render Markdown.</p>") : rendered;
}

MarkdownRenderResult renderMarkdownDocumentWithStyle(quint64 requestId,
                                                     const QString &markdown,
                                                     const QString &filePath,
                                                     const MarkdownRenderStyle &style,
                                                     const std::shared_ptr<std::atomic_bool> &cancellation = {})
{
    MarkdownRenderResult result;
    result.requestId = requestId;
    result.filePath = filePath;
    const QString body = markdownToHtmlBody(markdown, cancellation);
    if (!cancellation || !cancellation->load(std::memory_order_relaxed))
    {
        result.htmlDocument = buildHtmlDocumentFromStyle(body, style);
    }
    return result;
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
    if (payload.size() > MaximumPreviewResourceSize * 2)
        return {};
    if (meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive))
    {
        const QByteArray decoded = QByteArray::fromBase64(payload);
        return decoded.size() <= MaximumPreviewResourceSize ? decoded : QByteArray();
    }

    const QByteArray decoded = QByteArray::fromPercentEncoding(payload);
    return decoded.size() <= MaximumPreviewResourceSize ? decoded : QByteArray();
}

bool isInsideDirectory(const QString &filePath, const QString &rootPath)
{
    return DocumentPath::isInsideDirectory(filePath, rootPath);
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
    if (file.size() < 0 || file.size() > MaximumPreviewResourceSize)
        return {};

    const QByteArray content = file.read(MaximumPreviewResourceSize + 1);
    return content.size() <= MaximumPreviewResourceSize ? content : QByteArray();
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

        const QString scheme = url.scheme().toLower();
        if (url.isLocalFile()
            || scheme == QLatin1String("http")
            || scheme == QLatin1String("https")
            || scheme == QLatin1String("mailto"))
        {
            QDesktopServices::openUrl(url);
        }
    });
    updatePreview(preview, markdown, filePath);
    return preview;
}

void MarkdownPreviewer::updatePreview(QLiteHtmlWidget *preview, const QString &markdown, const QString &filePath)
{
    if (!preview) return;

    applyRenderedPreview(preview, renderMarkdownDocument(0, markdown, filePath, RenderMode::Screen));
}

void MarkdownPreviewer::applyRenderedPreview(QLiteHtmlWidget *preview, const MarkdownRenderResult &result)
{
    if (!preview) return;

    QString rootPath;
    if (!result.filePath.isEmpty())
    {
        const QFileInfo info(result.filePath);
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

    preview->setHtml(result.htmlDocument);

    QPalette vpPalette = preview->viewport()->palette();
    vpPalette.setColor(QPalette::Base, ThemeColors::MarkdownPreview::bodyBg());
    vpPalette.setColor(QPalette::Window, ThemeColors::MarkdownPreview::bodyBg());
    preview->viewport()->setPalette(vpPalette);
    preview->viewport()->setAutoFillBackground(true);
}

quint64 MarkdownPreviewer::renderPreviewAsync(const QString &markdown,
                                               const QString &filePath,
                                               QObject *requestOwner)
{
    const quint64 requestId = m_nextRequestId++;
    QObject *owner = requestOwner ? requestOwner : this;
    const quintptr ownerKey = reinterpret_cast<quintptr>(owner);
    const PendingRender render{owner,
                               requestId,
                               markdown,
                               filePath,
                               std::make_shared<std::atomic_bool>(false)};
    if (m_renderActive)
    {
        if (m_activeOwner == owner && m_activeCancellation)
        {
            m_activeCancellation->store(true, std::memory_order_relaxed);
        }
        if (!m_pendingRenders.contains(ownerKey))
            m_pendingOrder.enqueue(ownerKey);
        m_pendingRenders.insert(ownerKey, render);
        return requestId;
    }

    startAsyncRender(render);
    return requestId;
}

void MarkdownPreviewer::startAsyncRender(const PendingRender &render)
{
    m_renderActive = true;
    m_activeOwner = render.owner;
    m_activeCancellation = render.cancellation;
    const MarkdownRenderStyle style = captureMarkdownRenderStyle(MarkdownRenderMode::Screen);
    const quint64 requestId = render.requestId;
    const QString markdown = render.markdown;
    const QString filePath = render.filePath;
    const std::shared_ptr<std::atomic_bool> cancellation = render.cancellation;
    auto *watcher = new QFutureWatcher<MarkdownRenderResult>(this);
    connect(watcher, &QFutureWatcher<MarkdownRenderResult>::finished, this, [this, watcher]() {
        const MarkdownRenderResult result = watcher->result();
        watcher->deleteLater();
        // Keep the active flag set while notifying consumers. A consumer may
        // request another render from its slot; that request must join the
        // coalescing queue instead of starting a second concurrent worker.
        emit previewRendered(result);
        m_renderActive = false;
        m_activeOwner.clear();
        m_activeCancellation.reset();
        startNextPendingRender();
    });
    watcher->setFuture(QtConcurrent::run(BackgroundWorkScheduler::backgroundPool(), [requestId, markdown, filePath, style, cancellation]() {
        return renderMarkdownDocumentWithStyle(requestId,
                                               markdown,
                                               filePath,
                                               style,
                                               cancellation);
    }));
}

void MarkdownPreviewer::startNextPendingRender()
{
    while (!m_pendingOrder.isEmpty())
    {
        const quintptr ownerKey = m_pendingOrder.dequeue();
        const PendingRender render = m_pendingRenders.take(ownerKey);
        if (render.owner)
        {
            startAsyncRender(render);
            return;
        }
    }
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
    preview.setHtml(renderMarkdownDocument(0, markdown, filePath, RenderMode::Pdf).htmlDocument);

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

MarkdownRenderResult MarkdownPreviewer::renderMarkdownDocument(quint64 requestId,
                                                               const QString &markdown,
                                                               const QString &filePath,
                                                               RenderMode mode) const
{
    const MarkdownRenderMode renderMode = mode == RenderMode::Pdf ? MarkdownRenderMode::Pdf : MarkdownRenderMode::Screen;
    return renderMarkdownDocumentWithStyle(requestId, markdown, filePath, captureMarkdownRenderStyle(renderMode));
}

QString MarkdownPreviewer::buildHtmlDocument(const QString &bodyHtml, RenderMode mode) const
{
    const MarkdownRenderMode renderMode = mode == RenderMode::Pdf ? MarkdownRenderMode::Pdf : MarkdownRenderMode::Screen;
    return buildHtmlDocumentFromStyle(bodyHtml, captureMarkdownRenderStyle(renderMode));
}
