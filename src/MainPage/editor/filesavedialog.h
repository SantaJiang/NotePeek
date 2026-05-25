#ifndef FILESAVEDIALOG_H
#define FILESAVEDIALOG_H

#include <QDialog>
#include <QList>
#include <QMap>

class QCheckBox;
class CustomEditor;

class FileSaveDialog : public QDialog
{
    Q_OBJECT

public:
    enum { DontSave = 2 };

    explicit FileSaveDialog(const QMap<CustomEditor*, QString> &files, QWidget *parent = nullptr);
    QList<CustomEditor*> selectedEditors() const;

private:
    QList<QCheckBox*> m_checkboxes;
    QMap<QCheckBox*, CustomEditor*> m_editorMap;
};

#endif // FILESAVEDIALOG_H
