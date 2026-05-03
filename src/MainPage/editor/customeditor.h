#ifndef CUSTOMEDITOR_H
#define CUSTOMEDITOR_H

#include <QObject>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "../../QScintilla/Qsci/qsciscintilla.h"

class CustomEditor : public QsciScintilla
{
    Q_OBJECT
public:
    explicit CustomEditor(QWidget *parent = nullptr);

    void setFilePath(const QString &path);
    QString filePath() const;

signals:
    void filesDropped(const QList<QUrl> &urls);

protected:
    void dragEnterEvent(QDragEnterEvent  *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    QString m_filePath;
};

#endif // CUSTOMEDITOR_H
