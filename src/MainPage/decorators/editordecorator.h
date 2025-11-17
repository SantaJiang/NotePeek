#ifndef EDITORDECORATOR_H
#define EDITORDECORATOR_H

#include <QObject>

class QsciScintilla;

class EditorDecorator : public QObject
{
    Q_OBJECT

public:
    explicit EditorDecorator(QsciScintilla *editor);
    virtual ~EditorDecorator();

    void setEnabled(bool b);
    bool isEnabled() const { return enabled; }

    virtual void notify(const void *notificationData = nullptr) = 0;

signals:
    void stateChanged(bool enabled);

protected:
    QsciScintilla *editor;
    bool enabled;
};

#endif // EDITORDECORATOR_H
