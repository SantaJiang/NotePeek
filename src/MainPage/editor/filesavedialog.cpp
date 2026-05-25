#include "filesavedialog.h"
#include "customeditor.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

FileSaveDialog::FileSaveDialog(const QMap<CustomEditor*, QString> &files, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Save Changes"));
    setModal(true);
    setMinimumSize(500, 300);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *label = new QLabel(tr("The following files have unsaved changes:"));
    mainLayout->addWidget(label);

    QScrollArea *scrollArea = new QScrollArea;
    QWidget *scrollWidget = new QWidget;
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

    QList<CustomEditor*> editors = files.keys();
    for (CustomEditor *editor : editors)
    {
        QCheckBox *checkBox = new QCheckBox(files.value(editor));
        checkBox->setChecked(true);
        scrollLayout->addWidget(checkBox);
        m_checkboxes.append(checkBox);
        m_editorMap.insert(checkBox, editor);
    }

    scrollLayout->addStretch();
    scrollArea->setWidget(scrollWidget);
    scrollArea->setWidgetResizable(true);
    mainLayout->addWidget(scrollArea);

    QHBoxLayout *buttonLayout = new QHBoxLayout;

    QPushButton *saveSelectedButton = new QPushButton(tr("Save Selected"));
    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    QPushButton *dontSaveButton = new QPushButton(tr("Do Not Save"));

    buttonLayout->addWidget(saveSelectedButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(dontSaveButton);

    mainLayout->addLayout(buttonLayout);

    connect(saveSelectedButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(dontSaveButton, &QPushButton::clicked, this, [this]()
    {
        done(FileSaveDialog::DontSave);
    });
}

QList<CustomEditor*> FileSaveDialog::selectedEditors() const
{
    QList<CustomEditor*> editors;
    for (QCheckBox *checkBox : m_checkboxes)
    {
        if (checkBox->isChecked())
        {
            editors.append(m_editorMap.value(checkBox));
        }
    }
    return editors;
}
