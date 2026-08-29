#include "editor/editorsnapshotencoder.h"

#include "editor/customeditor.h"
#include "editor/documentformat.h"

#include <QCoreApplication>
#include <Qsci/qsciscintillabase.h>

#include <cstring>
#include <limits>

bool EditorSnapshotEncoder::encode(CustomEditor *editor,
                                   QByteArray *encodedContent,
                                   QString *errorMessage)
{
    if (!editor || !encodedContent)
    {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate(
                "DocumentPersistenceService", "No editor content to save.");
        return false;
    }

    const auto length = editor->SendScintilla(QsciScintillaBase::SCI_GETLENGTH);
    if (length > std::numeric_limits<int>::max())
    {
        if (errorMessage)
        {
            *errorMessage = QCoreApplication::translate(
                "DocumentPersistenceService",
                "The document is too large to snapshot for saving.");
        }
        return false;
    }

    const DocumentFormat format = editor->documentFormat();
    const char *buffer = reinterpret_cast<const char *>(
        editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETCHARACTERPOINTER));
    if ((format.encoding == DocumentEncoding::Utf8
         || format.encoding == DocumentEncoding::Utf8Bom)
        && (format.lineEnding == DocumentLineEnding::Lf
            || format.lineEnding == DocumentLineEnding::Mixed))
    {
        if (!buffer && length > 0)
        {
            if (errorMessage)
                *errorMessage = QCoreApplication::translate(
                    "DocumentPersistenceService", "Cannot access editor buffer.");
            return false;
        }
        const int bomSize = format.encoding == DocumentEncoding::Utf8Bom ? 3 : 0;
        encodedContent->resize(bomSize + int(length));
        if (bomSize != 0)
            std::memcpy(encodedContent->data(), "\xef\xbb\xbf", 3);
        if (length > 0)
            std::memcpy(encodedContent->data() + bomSize, buffer, size_t(length));
    }
    else
    {
        // Convert directly from Scintilla's buffer; text() would add another
        // whole-document copy before encoding.
        const QString text = buffer ? QString::fromUtf8(buffer, int(length))
                                    : editor->text();
        *encodedContent = DocumentFormatCodec::encode(text, format);
    }
    return true;
}
