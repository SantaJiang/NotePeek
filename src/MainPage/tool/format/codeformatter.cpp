#include "tool/format/codeformatter.h"

#include <NotePeek/lexerfactory.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVector>
#include <QXmlStreamAttribute>
#include <QXmlStreamReader>

#include <mutex>

#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>

namespace {

QString indentUnit(const CodeFormatter::Options &options)
{
    return options.useTabs ? QStringLiteral("\t")
                           : QString(qMax(1, options.indentWidth), QLatin1Char(' '));
}

CodeFormatter::Result failure(const QString &error)
{
    CodeFormatter::Result result;
    result.ok = false;
    result.error = error;
    return result;
}

CodeFormatter::Result success(const QString &text)
{
    CodeFormatter::Result result;
    result.ok = true;
    result.text = text;
    return result;
}

// ---------------------------------------------------------------- JSON ----

// A hand-written printer rather than QJsonDocument: QJsonObject stores members
// sorted by key, so a round trip through it silently reorders every object in
// the document.  Streaming the source preserves member order, and keeps the
// original spelling of numbers and string escapes.
class JsonPrinter
{
public:
    JsonPrinter(const QString &source, const CodeFormatter::Options &options, bool singleLine)
        : m_source(source)
        , m_indent(indentUnit(options))
        , m_eol(options.eol)
        , m_singleLine(singleLine)
    {
    }

    bool run(QString *out)
    {
        skipWhitespace();
        if (m_pos >= m_source.size())
        {
            m_error = errorAt(CodeFormatter::tr("the document is empty"));
            return false;
        }
        if (!writeValue(0))
        {
            return false;
        }
        skipWhitespace();
        if (m_pos < m_source.size())
        {
            m_error = errorAt(CodeFormatter::tr("unexpected trailing content"));
            return false;
        }

        *out = m_out;
        return true;
    }

    QString error() const { return m_error; }

private:
    void skipWhitespace()
    {
        while (m_pos < m_source.size() && m_source.at(m_pos).isSpace())
        {
            ++m_pos;
        }
    }

    QChar peek() const
    {
        return m_pos < m_source.size() ? m_source.at(m_pos) : QChar();
    }

    QString errorAt(const QString &reason) const
    {
        int line = 1;
        int column = 1;
        for (qsizetype i = 0; i < m_pos && i < m_source.size(); ++i)
        {
            if (m_source.at(i) == QLatin1Char('\n'))
            {
                ++line;
                column = 1;
            }
            else
            {
                ++column;
            }
        }
        return CodeFormatter::tr("Line %1, column %2: %3")
            .arg(line)
            .arg(column)
            .arg(reason);
    }

    void newLine(int depth)
    {
        if (m_singleLine)
        {
            m_out += QLatin1Char(' ');
            return;
        }
        m_out += m_eol;
        m_out += m_indent.repeated(depth);
    }

    bool writeValue(int depth)
    {
        skipWhitespace();
        const QChar ch = peek();
        if (ch == QLatin1Char('{'))
        {
            return writeContainer(depth, QLatin1Char('{'), QLatin1Char('}'), true);
        }
        if (ch == QLatin1Char('['))
        {
            return writeContainer(depth, QLatin1Char('['), QLatin1Char(']'), false);
        }
        if (ch == QLatin1Char('"'))
        {
            return writeString();
        }
        if (ch == QLatin1Char('t') || ch == QLatin1Char('f') || ch == QLatin1Char('n'))
        {
            return writeKeyword();
        }
        if (ch == QLatin1Char('-') || ch.isDigit())
        {
            return writeNumber();
        }

        m_error = errorAt(CodeFormatter::tr("expected a JSON value"));
        return false;
    }

    bool writeContainer(int depth, QChar open, QChar close, bool members)
    {
        ++m_pos; // consume the opening bracket
        m_out += open;

        skipWhitespace();
        if (peek() == close)
        {
            ++m_pos;
            m_out += close;
            return true;
        }

        for (;;)
        {
            newLine(depth + 1);
            if (members)
            {
                skipWhitespace();
                if (peek() != QLatin1Char('"'))
                {
                    m_error = errorAt(CodeFormatter::tr("expected a member name"));
                    return false;
                }
                if (!writeString())
                {
                    return false;
                }
                skipWhitespace();
                if (peek() != QLatin1Char(':'))
                {
                    m_error = errorAt(CodeFormatter::tr("expected ':' after a member name"));
                    return false;
                }
                ++m_pos;
                m_out += QStringLiteral(": ");
            }

            if (!writeValue(depth + 1))
            {
                return false;
            }

            skipWhitespace();
            const QChar next = peek();
            if (next == QLatin1Char(','))
            {
                ++m_pos;
                m_out += QLatin1Char(',');
                continue;
            }
            if (next == close)
            {
                ++m_pos;
                newLine(depth);
                m_out += close;
                return true;
            }

            m_error = errorAt(CodeFormatter::tr("expected ',' or a closing bracket"));
            return false;
        }
    }

    bool writeString()
    {
        const qsizetype start = m_pos;
        ++m_pos; // consume the opening quote
        while (m_pos < m_source.size())
        {
            const QChar ch = m_source.at(m_pos);
            if (ch == QLatin1Char('\\'))
            {
                m_pos += 2;
                continue;
            }
            if (ch == QLatin1Char('"'))
            {
                ++m_pos;
                m_out += m_source.mid(start, m_pos - start);
                return true;
            }
            if (ch == QLatin1Char('\n'))
            {
                break;
            }
            ++m_pos;
        }

        m_pos = start;
        m_error = errorAt(CodeFormatter::tr("unterminated string"));
        return false;
    }

    bool writeKeyword()
    {
        for (const QString &keyword : { QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null") })
        {
            if (QStringView(m_source).mid(m_pos).startsWith(keyword))
            {
                m_out += keyword;
                m_pos += keyword.size();
                return true;
            }
        }

        m_error = errorAt(CodeFormatter::tr("expected true, false or null"));
        return false;
    }

    bool writeNumber()
    {
        const qsizetype start = m_pos;
        if (peek() == QLatin1Char('-'))
        {
            ++m_pos;
        }
        while (m_pos < m_source.size())
        {
            const QChar ch = m_source.at(m_pos);
            const bool exponentSign = (ch == QLatin1Char('+') || ch == QLatin1Char('-'))
                                      && m_pos > start
                                      && (m_source.at(m_pos - 1) == QLatin1Char('e')
                                          || m_source.at(m_pos - 1) == QLatin1Char('E'));
            if (ch.isDigit() || ch == QLatin1Char('.') || ch == QLatin1Char('e')
                || ch == QLatin1Char('E') || exponentSign)
            {
                ++m_pos;
                continue;
            }
            break;
        }

        if (m_pos == start || (m_pos == start + 1 && m_source.at(start) == QLatin1Char('-')))
        {
            m_error = errorAt(CodeFormatter::tr("expected a number"));
            return false;
        }

        m_out += m_source.mid(start, m_pos - start);
        return true;
    }

    const QString &m_source;
    QString m_indent;
    QString m_eol;
    bool m_singleLine;
    qsizetype m_pos = 0;
    QString m_out;
    QString m_error;
};

CodeFormatter::Result formatJson(const QString &text, const CodeFormatter::Options &options)
{
    JsonPrinter printer(text, options, false);
    QString formatted;
    if (!printer.run(&formatted))
    {
        return failure(printer.error());
    }
    return success(formatted + options.eol);
}

// JSON Lines carries one value per line, so the value itself is collapsed onto
// a single line instead of being expanded; the line structure is the format.
CodeFormatter::Result formatJsonLines(const QString &text, const CodeFormatter::Options &options)
{
    QStringList formattedLines;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")));
    for (qsizetype index = 0; index < lines.size(); ++index)
    {
        const QString line = lines.at(index).trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        JsonPrinter printer(line, options, true);
        QString formatted;
        if (!printer.run(&formatted))
        {
            return failure(CodeFormatter::tr("Line %1 is not valid JSON: %2")
                               .arg(index + 1)
                               .arg(printer.error()));
        }
        formattedLines.append(formatted);
    }

    if (formattedLines.isEmpty())
    {
        return success(text);
    }
    return success(formattedLines.join(options.eol) + options.eol);
}

// ----------------------------------------------------------------- XML ----

struct XmlNode
{
    enum Kind
    {
        Element,
        Text,
        CData,
        Comment,
        ProcessingInstruction
    };

    Kind kind = Element;
    QString name;
    QString text;
    QVector<QPair<QString, QString>> attributes;
    QVector<XmlNode> children;
};

QString escapedXmlText(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return escaped;
}

QString escapedXmlAttribute(const QString &value)
{
    QString escaped = escapedXmlText(value);
    escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    // A literal newline or tab in an attribute is normalised to a space by every
    // XML reader, so writing it back verbatim would change the value.
    escaped.replace(QLatin1Char('\t'), QStringLiteral("&#9;"));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("&#10;"));
    escaped.replace(QLatin1Char('\r'), QStringLiteral("&#13;"));
    return escaped;
}

// xml:space="preserve" makes every space inside the element part of the
// content, including the whitespace-only text this formatter would otherwise
// replace with indentation.
bool preservesWhitespace(const XmlNode &node)
{
    for (const auto &attribute : node.attributes)
    {
        if (attribute.first == QStringLiteral("xml:space"))
        {
            return attribute.second.trimmed() == QStringLiteral("preserve");
        }
    }
    return false;
}

// True when the element mixes text with markup (<p>a <b>b</b></p>).  Adding
// indentation there would change the rendered content, so such elements are
// written on one line exactly as they were authored.
bool hasSignificantText(const XmlNode &node)
{
    for (const XmlNode &child : node.children)
    {
        if (child.kind == XmlNode::CData)
        {
            return true;
        }
        if (child.kind == XmlNode::Text && !child.text.trimmed().isEmpty())
        {
            return true;
        }
    }
    return false;
}

QString openingTag(const XmlNode &node, bool selfClosing)
{
    QString tag = QLatin1Char('<') + node.name;
    for (const auto &attribute : node.attributes)
    {
        tag += QStringLiteral(" %1=\"%2\"").arg(attribute.first, escapedXmlAttribute(attribute.second));
    }
    tag += selfClosing ? QStringLiteral("/>") : QStringLiteral(">");
    return tag;
}

QString renderXmlInline(const XmlNode &node)
{
    switch (node.kind)
    {
    case XmlNode::Text:
        return escapedXmlText(node.text);
    case XmlNode::CData:
        return QStringLiteral("<![CDATA[%1]]>").arg(node.text);
    case XmlNode::Comment:
        return QStringLiteral("<!--%1-->").arg(node.text);
    case XmlNode::ProcessingInstruction:
        return QStringLiteral("<?%1 %2?>").arg(node.name, node.text);
    case XmlNode::Element:
        break;
    }

    if (node.children.isEmpty())
    {
        return openingTag(node, true);
    }

    QString rendered = openingTag(node, false);
    for (const XmlNode &child : node.children)
    {
        rendered += renderXmlInline(child);
    }
    rendered += QStringLiteral("</%1>").arg(node.name);
    return rendered;
}

void renderXmlNode(const XmlNode &node,
                   int depth,
                   const QString &indent,
                   const QString &eol,
                   QString *out)
{
    const QString prefix = indent.repeated(depth);
    if (node.kind != XmlNode::Element)
    {
        const QString content = renderXmlInline(node).trimmed();
        if (!content.isEmpty())
        {
            *out += prefix + content + eol;
        }
        return;
    }

    if (preservesWhitespace(node))
    {
        *out += prefix + renderXmlInline(node) + eol;
        return;
    }

    QVector<XmlNode> children;
    for (const XmlNode &child : node.children)
    {
        // Whitespace-only text between elements is layout, not content, and is
        // replaced by the indentation this formatter emits.
        if (child.kind == XmlNode::Text && child.text.trimmed().isEmpty())
        {
            continue;
        }
        children.append(child);
    }

    if (children.isEmpty())
    {
        *out += prefix + openingTag(node, true) + eol;
        return;
    }

    if (hasSignificantText(node))
    {
        *out += prefix + renderXmlInline(node).trimmed() + eol;
        return;
    }

    *out += prefix + openingTag(node, false) + eol;
    for (const XmlNode &child : children)
    {
        renderXmlNode(child, depth + 1, indent, eol, out);
    }
    *out += prefix + QStringLiteral("</%1>").arg(node.name) + eol;
}

CodeFormatter::Result formatXml(const QString &text, const CodeFormatter::Options &options)
{
    QXmlStreamReader reader(text);
    // Namespace processing rewrites prefixes and hides xmlns attributes.  A
    // formatter must hand back the author's own spelling, so names are read
    // qualified and xmlns declarations stay ordinary attributes.
    reader.setNamespaceProcessing(false);

    QString prologue;
    QVector<XmlNode> roots;
    // Open elements, innermost last.  Finished elements are moved into their
    // parent on EndElement, so no node is ever referenced by pointer while the
    // vectors holding it can still reallocate.
    QVector<XmlNode> stack;

    const auto appendNode = [&](const XmlNode &node) {
        if (stack.isEmpty())
        {
            roots.append(node);
            return;
        }
        stack.last().children.append(node);
    };

    while (!reader.atEnd())
    {
        switch (reader.readNext())
        {
        case QXmlStreamReader::StartDocument:
        {
            // documentVersion() is empty when the document carries no XML
            // declaration.  A formatter must not invent one, so the declaration
            // is rebuilt from exactly the parts the author wrote.
            if (reader.documentVersion().isEmpty())
            {
                break;
            }
            prologue += QStringLiteral("<?xml version=\"%1\"").arg(reader.documentVersion().toString());
            if (!reader.documentEncoding().isEmpty())
            {
                prologue += QStringLiteral(" encoding=\"%1\"").arg(reader.documentEncoding().toString());
            }
            if (reader.isStandaloneDocument())
            {
                prologue += QStringLiteral(" standalone=\"yes\"");
            }
            prologue += QStringLiteral("?>") + options.eol;
            break;
        }
        case QXmlStreamReader::DTD:
            prologue += reader.text().toString().trimmed() + options.eol;
            break;
        case QXmlStreamReader::StartElement:
        {
            XmlNode node;
            node.kind = XmlNode::Element;
            node.name = reader.qualifiedName().toString();
            for (const QXmlStreamAttribute &attribute : reader.attributes())
            {
                node.attributes.append({ attribute.qualifiedName().toString(), attribute.value().toString() });
            }
            stack.append(node);
            break;
        }
        case QXmlStreamReader::EndElement:
            if (!stack.isEmpty())
            {
                const XmlNode finished = stack.takeLast();
                appendNode(finished);
            }
            break;
        case QXmlStreamReader::Characters:
        {
            XmlNode node;
            node.kind = reader.isCDATA() ? XmlNode::CData : XmlNode::Text;
            node.text = reader.text().toString();
            if (node.kind == XmlNode::Text && node.text.trimmed().isEmpty() && stack.isEmpty())
            {
                break;
            }
            appendNode(node);
            break;
        }
        case QXmlStreamReader::Comment:
        {
            XmlNode node;
            node.kind = XmlNode::Comment;
            node.text = reader.text().toString();
            appendNode(node);
            break;
        }
        case QXmlStreamReader::ProcessingInstruction:
        {
            XmlNode node;
            node.kind = XmlNode::ProcessingInstruction;
            node.name = reader.processingInstructionTarget().toString();
            node.text = reader.processingInstructionData().toString();
            appendNode(node);
            break;
        }
        default:
            break;
        }
    }

    if (reader.hasError())
    {
        return failure(CodeFormatter::tr("Line %1, column %2: %3")
                           .arg(reader.lineNumber())
                           .arg(reader.columnNumber())
                           .arg(reader.errorString()));
    }

    QString formatted = prologue;
    const QString indent = indentUnit(options);
    for (const XmlNode &root : roots)
    {
        renderXmlNode(root, 0, indent, options.eol, &formatted);
    }

    if (formatted.trimmed().isEmpty())
    {
        return failure(CodeFormatter::tr("the document contains no XML markup"));
    }
    return success(formatted);
}

// ----------------------------------------------------------------- CSS ----

class CssPrinter
{
public:
    CssPrinter(const QString &source, const CodeFormatter::Options &options)
        : m_source(source)
        , m_indent(indentUnit(options))
        , m_eol(options.eol)
    {
    }

    QString run()
    {
        while (m_pos < m_source.size())
        {
            const QChar ch = m_source.at(m_pos);
            if (ch == QLatin1Char('/') && next() == QLatin1Char('*'))
            {
                takeBlockComment();
                continue;
            }
            // "//" is not valid CSS, but preprocessor dialects saved as .css use
            // it.  Treating it as a comment keeps such a file readable instead of
            // folding the following selector into the comment text.
            if (ch == QLatin1Char('/') && next() == QLatin1Char('/'))
            {
                takeLineComment();
                continue;
            }
            if (ch == QLatin1Char('"') || ch == QLatin1Char('\''))
            {
                takeQuoted(ch);
                continue;
            }
            if (ch == QLatin1Char('('))
            {
                takeParenthesised();
                continue;
            }
            if (ch == QLatin1Char('{'))
            {
                openBlock();
                continue;
            }
            if (ch == QLatin1Char('}'))
            {
                closeBlock();
                continue;
            }
            if (ch == QLatin1Char(';'))
            {
                ++m_pos;
                flushStatement(QLatin1Char(';'));
                continue;
            }
            if (ch.isSpace())
            {
                appendSpace();
                ++m_pos;
                continue;
            }

            m_statement += ch;
            ++m_pos;
        }

        flushStatement(QChar());
        while (m_out.endsWith(m_eol + m_eol))
        {
            m_out.chop(m_eol.size());
        }
        return m_out;
    }

private:
    QChar next() const
    {
        return m_pos + 1 < m_source.size() ? m_source.at(m_pos + 1) : QChar();
    }

    void appendSpace()
    {
        if (!m_statement.isEmpty() && !m_statement.endsWith(QLatin1Char(' ')))
        {
            m_statement += QLatin1Char(' ');
        }
    }

    void takeQuoted(QChar quote)
    {
        m_statement += m_source.at(m_pos);
        ++m_pos;
        while (m_pos < m_source.size())
        {
            const QChar ch = m_source.at(m_pos);
            m_statement += ch;
            ++m_pos;
            if (ch == QLatin1Char('\\') && m_pos < m_source.size())
            {
                m_statement += m_source.at(m_pos);
                ++m_pos;
                continue;
            }
            if (ch == quote)
            {
                return;
            }
        }
    }

    // url(...) and calc(...) may contain characters that are structural
    // elsewhere, so their contents are copied through untouched apart from
    // whitespace collapsing.
    void takeParenthesised()
    {
        int depth = 0;
        while (m_pos < m_source.size())
        {
            const QChar ch = m_source.at(m_pos);
            if (ch == QLatin1Char('"') || ch == QLatin1Char('\''))
            {
                takeQuoted(ch);
                continue;
            }
            if (ch.isSpace())
            {
                appendSpace();
                ++m_pos;
                continue;
            }

            m_statement += ch;
            ++m_pos;
            if (ch == QLatin1Char('('))
            {
                ++depth;
            }
            else if (ch == QLatin1Char(')'))
            {
                if (--depth == 0)
                {
                    return;
                }
            }
        }
    }

    void takeBlockComment()
    {
        const qsizetype start = m_pos;
        m_pos += 2;
        while (m_pos < m_source.size()
               && !(m_source.at(m_pos) == QLatin1Char('*') && next() == QLatin1Char('/')))
        {
            ++m_pos;
        }
        m_pos = qMin(m_pos + 2, qsizetype(m_source.size()));
        emitComment(m_source.mid(start, m_pos - start));
    }

    void takeLineComment()
    {
        const qsizetype start = m_pos;
        while (m_pos < m_source.size() && m_source.at(m_pos) != QLatin1Char('\n'))
        {
            ++m_pos;
        }
        emitComment(m_source.mid(start, m_pos - start).trimmed());
    }

    // A comment that starts a line keeps its own line; one trailing a value stays
    // attached to the statement it annotates.
    void emitComment(const QString &comment)
    {
        if (m_statement.trimmed().isEmpty())
        {
            m_statement.clear();
            writeLine(comment);
            return;
        }

        appendSpace();
        m_statement += comment;
    }

    void openBlock()
    {
        ++m_pos;
        const QString selector = m_statement.trimmed();
        m_statement.clear();
        writeLine(selector.isEmpty() ? QStringLiteral("{") : selector + QStringLiteral(" {"));
        ++m_depth;
    }

    void closeBlock()
    {
        ++m_pos;
        // Anything still pending at a closing brace is the block's last
        // declaration written without its optional semicolon; the formatter
        // adds it so every declaration in the block looks the same.
        flushStatement(QLatin1Char(';'));
        m_depth = qMax(0, m_depth - 1);
        writeLine(QStringLiteral("}"));
        if (m_depth == 0)
        {
            m_out += m_eol;
        }
    }

    void flushStatement(QChar terminator)
    {
        QString statement = m_statement.trimmed();
        m_statement.clear();
        if (statement.isEmpty())
        {
            return;
        }

        if (terminator == QLatin1Char(';'))
        {
            // Only a declaration can be terminated by ';', so the first colon
            // is a property separator and never a pseudo-class.
            const qsizetype colon = statement.indexOf(QLatin1Char(':'));
            if (colon > 0)
            {
                statement = statement.left(colon).trimmed()
                            + QStringLiteral(": ")
                            + statement.mid(colon + 1).trimmed();
            }
            statement += QLatin1Char(';');
        }

        writeLine(statement);
    }

    void writeLine(const QString &line)
    {
        m_out += m_indent.repeated(m_depth) + line + m_eol;
    }

    const QString &m_source;
    QString m_indent;
    QString m_eol;
    qsizetype m_pos = 0;
    int m_depth = 0;
    QString m_statement;
    QString m_out;
};

CodeFormatter::Result formatCss(const QString &text, const CodeFormatter::Options &options)
{
    return success(CssPrinter(text, options).run());
}

// ------------------------------------------------------------ Markdown ----

CodeFormatter::Result formatMarkdown(const QString &text, const CodeFormatter::Options &options)
{
    // cmark renders an empty document as an empty string, which would otherwise
    // be reported as a rendering failure.
    if (text.trimmed().isEmpty())
    {
        return success(text);
    }

    static std::once_flag extensionsRegistered;
    std::call_once(extensionsRegistered, []() {
        cmark_gfm_core_extensions_ensure_registered();
    });

    const int cmarkOptions = CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE;
    cmark_parser *parser = cmark_parser_new(cmarkOptions);
    if (!parser)
    {
        return failure(CodeFormatter::tr("could not start the Markdown parser"));
    }

    for (const char *name : { "table", "tasklist", "strikethrough", "autolink" })
    {
        if (cmark_syntax_extension *extension = cmark_find_syntax_extension(name))
        {
            cmark_parser_attach_syntax_extension(parser, extension);
        }
    }

    const QByteArray input = text.toUtf8();
    cmark_parser_feed(parser, input.constData(), static_cast<size_t>(input.size()));
    cmark_node *document = cmark_parser_finish(parser);
    if (!document)
    {
        cmark_parser_free(parser);
        return failure(CodeFormatter::tr("could not parse the Markdown document"));
    }

    // Width 0 disables re-wrapping: paragraphs keep the author's line breaks
    // and only block structure, list markers and escaping are normalised.
    char *rendered = cmark_render_commonmark(document, cmarkOptions, 0);
    QString formatted;
    if (rendered)
    {
        formatted = QString::fromUtf8(rendered);
        cmark_get_default_mem_allocator()->free(rendered);
    }
    cmark_node_free(document);
    cmark_parser_free(parser);

    if (formatted.isEmpty())
    {
        return failure(CodeFormatter::tr("could not render the Markdown document"));
    }

    if (options.eol != QStringLiteral("\n"))
    {
        formatted.replace(QStringLiteral("\n"), options.eol);
    }
    return success(formatted);
}

// -------------------------------------------------------- clang-format ----

bool isClangFormatLanguage(LexerFactory::Language language)
{
    switch (language)
    {
    case LexerFactory::CPP:
    case LexerFactory::Java:
    case LexerFactory::JavaScript:
    case LexerFactory::CSHARP:
        return true;
    default:
        return false;
    }
}

CodeFormatter::Result runClangFormat(const QString &text, const QString &filePath)
{
    const QString program = CodeFormatter::clangFormatPath();
    if (program.isEmpty())
    {
        return failure(CodeFormatter::tr("clang-format was not found on PATH."));
    }

    const QFileInfo fileInfo(filePath);
    QProcess process;
    // -assume-filename lets clang-format pick the language from the extension
    // and, together with the working directory, locate the project's own
    // .clang-format file.  The buffer is piped in so unsaved edits are
    // formatted, not the copy on disk.
    process.setProgram(program);
    process.setArguments({ QStringLiteral("-assume-filename=%1").arg(fileInfo.fileName()) });
    if (fileInfo.absoluteDir().exists())
    {
        process.setWorkingDirectory(fileInfo.absolutePath());
    }
    process.start();
    if (!process.waitForStarted(5000))
    {
        return failure(CodeFormatter::tr("clang-format could not be started."));
    }

    process.write(text.toUtf8());
    process.closeWriteChannel();
    if (!process.waitForFinished(15000))
    {
        process.kill();
        process.waitForFinished(1000);
        return failure(CodeFormatter::tr("clang-format did not finish in time."));
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        const QString diagnostics = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return failure(diagnostics.isEmpty()
                           ? CodeFormatter::tr("clang-format reported an error.")
                           : diagnostics);
    }

    const QByteArray output = process.readAllStandardOutput();
    if (output.isEmpty())
    {
        return failure(CodeFormatter::tr("clang-format produced no output."));
    }
    return success(QString::fromUtf8(output));
}

} // namespace

QString CodeFormatter::clangFormatPath()
{
    static const QString cached = QStandardPaths::findExecutable(QStringLiteral("clang-format"));
    return cached;
}

QString CodeFormatter::formatterName(const QString &filePath)
{
    if (filePath.isEmpty())
    {
        return QString();
    }

    const LexerFactory::Language language = LexerFactory::languageFromFilePath(filePath);
    switch (language)
    {
    case LexerFactory::JSON:
        return QStringLiteral("JSON");
    case LexerFactory::JSONL:
        return QStringLiteral("JSON Lines");
    case LexerFactory::XML:
        return QStringLiteral("XML");
    case LexerFactory::MARKDOWN:
        return QStringLiteral("Markdown");
    case LexerFactory::CSS:
        return QStringLiteral("CSS");
    default:
        break;
    }

    if (isClangFormatLanguage(language) && !clangFormatPath().isEmpty())
    {
        return QStringLiteral("clang-format");
    }
    return QString();
}

bool CodeFormatter::canFormat(const QString &filePath)
{
    return !formatterName(filePath).isEmpty();
}

CodeFormatter::Result CodeFormatter::format(const QString &text,
                                            const QString &filePath,
                                            const Options &options)
{
    const LexerFactory::Language language = LexerFactory::languageFromFilePath(filePath);
    switch (language)
    {
    case LexerFactory::JSON:
        return formatJson(text, options);
    case LexerFactory::JSONL:
        return formatJsonLines(text, options);
    case LexerFactory::XML:
        return formatXml(text, options);
    case LexerFactory::MARKDOWN:
        return formatMarkdown(text, options);
    case LexerFactory::CSS:
        return formatCss(text, options);
    default:
        break;
    }

    if (isClangFormatLanguage(language))
    {
        return runClangFormat(text, filePath);
    }
    return failure(CodeFormatter::tr("This file type has no formatter."));
}
