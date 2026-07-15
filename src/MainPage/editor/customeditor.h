#ifndef CUSTOMEDITOR_H
#define CUSTOMEDITOR_H

#include <QObject>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <Qsci/qsciscintilla.h>

class CustomEditor : public QsciScintilla
{
    Q_OBJECT
public:
    explicit CustomEditor(QWidget *parent = nullptr);

    void setFilePath(const QString &path);
    QString filePath() const;
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
    bool m_codeEditingFeaturesEnabled = false;
    bool m_bracketAssistanceEnabled = false;
    bool m_documentCompletionEnabled = false;
};

#endif // CUSTOMEDITOR_H
