#include "filesavedialog.h"
#include "../QScintilla/Qsci/qsciscintilla.h"

FileSaveDialog::FileSaveDialog(const QMap<QsciScintilla*, QString> &files, QWidget *parent)
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

    QList<QsciScintilla*> editors = files.keys();
    for (QsciScintilla *editor : editors)
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
        done(2);
    }); // 使用2表示不保存..
}

QList<QsciScintilla*> FileSaveDialog::selectedEditors() const
{
    QList<QsciScintilla*> editors;
    for (QCheckBox *checkBox : m_checkboxes)
    {
        if (checkBox->isChecked())
        {
            editors.append(m_editorMap.value(checkBox));
        }
    }
    return editors;
}
