#ifndef CUSTOMEDITOR_H
#define CUSTOMEDITOR_H

#include <QObject>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <Qsci/qsciscintilla.h>
#include <QSharedPointer>

#include "editor/documentsession.h"

class CustomEditor : public QsciScintilla
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
    void setCodeEditingFeaturesEnabled(bool enabled);
    bool codeEditingFeaturesEnabled() const;
    void setBracketAssistanceEnabled(bool enabled);
    bool bracketAssistanceEnabled() const;
    void setDocumentCompletionEnabled(bool enabled);
    bool documentCompletionEnabled() const;
    void toggleCurrentFold();
    void toggleAllFolds();
    void goToMatchingBrace();
    void completeWordFromDocument();

signals:
    void filesDropped(const QList<QUrl> &urls);

protected:
    void dragEnterEvent(QDragEnterEvent  *e) override;
    void dropEvent(QDropEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    QString m_filePath;
    QSharedPointer<DocumentSession> m_documentSession = QSharedPointer<DocumentSession>::create();
    bool m_codeEditingFeaturesEnabled = false;
    bool m_bracketAssistanceEnabled = false;
    bool m_documentCompletionEnabled = false;
};

#endif // CUSTOMEDITOR_H
