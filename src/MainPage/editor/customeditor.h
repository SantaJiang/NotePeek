#ifndef CUSTOMEDITOR_H
#define CUSTOMEDITOR_H

#include <QObject>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <Qsci/notepeekeditor.h>
#include <QSharedPointer>

#include "editor/documentsession.h"

class CustomEditor : public NotePeek::Editor
{
    Q_OBJECT
public:
    explicit CustomEditor(QWidget *parent = nullptr);

    void setFilePath(const QString &path);
    QString filePath() const;
    void setDocumentFormat(const DocumentFormat &format);
    DocumentFormat documentFormat() const;
    void setExternalConflict(bool conflict, bool fileDeleted = false);
    bool hasExternalConflict() const;
    bool wasDeletedExternally() const;
    void shareDocumentSessionFrom(const CustomEditor *source);
    void setPersistenceState(DocumentPersistenceState state);
    DocumentPersistenceState persistenceState() const;
    bool isModified() const;
    void setModified(bool modified) override;
    // QsciScintilla::setModified(true) is intentionally a no-op. This keeps a
    // shared forced-dirty state for failed saves and external deletions.
    void markModified();
    void beginEncodingConversion(DocumentEncoding encoding);
    bool hasPendingEncodingConversion() const;
    void finishEncodingConversion(bool success);
    quint64 beginEncodingReload();
    bool isCurrentEncodingReload(quint64 generation) const;
signals:
    void filesDropped(const QList<QUrl> &urls);

protected:
    void dragEnterEvent(QDragEnterEvent  *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    QString m_filePath;
    QSharedPointer<DocumentSession> m_documentSession = QSharedPointer<DocumentSession>::create();
};

#endif // CUSTOMEDITOR_H
