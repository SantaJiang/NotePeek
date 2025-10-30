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

signals:
    void filesDropped(const QList<QUrl> &urls);  // 自定义多文件信号

protected:
    void dragEnterEvent(QDragEnterEvent  *e) override;
    void dropEvent(QDropEvent *e) override;

};

#endif // CUSTOMEDITOR_H
