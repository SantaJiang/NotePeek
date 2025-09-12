#ifndef FILESAVEDIALOG_H
#define FILESAVEDIALOG_H

#include <QDialog>
#include <QMap>
#include <QList>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QCheckBox>

class QsciScintilla;
class FileSaveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileSaveDialog(const QMap<QsciScintilla*, QString> &files, QWidget *parent = nullptr);
    QList<QsciScintilla*> selectedEditors() const;

private:
    QList<QCheckBox*> m_checkboxes;
    QMap<QCheckBox*, QsciScintilla*> m_editorMap;
};

#endif // FILESAVEDIALOG_H
