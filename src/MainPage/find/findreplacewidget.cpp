#include "findreplacewidget.h"
#include "ui_findreplacewidget.h"
#include "findreplacemanager.h"
#include "../editor/multieditor.h"
#include <QKeyEvent>
#include <QLineEdit>
#include <QFontDatabase>

FindReplaceWidget::FindReplaceWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FindReplaceWidget)
    , m_manager(nullptr)
    , m_multiEditor(nullptr)
{
    ui->setupUi(this);

    int fontId = QFontDatabase::addApplicationFont("://resource/fontawesome-webfont.ttf");
    QString fontName = QFontDatabase::applicationFontFamilies(fontId).at(0);
    QFont iconFont = QFont(fontName);
    ui->buttonClose->setFont(iconFont);
    ui->buttonClose->setText(QChar(0xf00d));

    // 连接 UI 信号到槽函数
    connect(ui->buttonFind, &QPushButton::clicked, this, &FindReplaceWidget::onFindClicked);
    connect(ui->buttonReplace, &QPushButton::clicked, this, &FindReplaceWidget::onReplaceClicked);
    connect(ui->buttonReplaceAll, &QPushButton::clicked, this, &FindReplaceWidget::onReplaceAllClicked);
    connect(ui->buttonClose, &QPushButton::clicked, this, &FindReplaceWidget::onCloseClicked);

    // 连接输入控件信号
    connect(ui->comboFind->lineEdit(), &QLineEdit::textChanged, this, &FindReplaceWidget::onFindTextChanged);
    connect(ui->comboReplace->lineEdit(), &QLineEdit::textChanged, this, &FindReplaceWidget::onReplaceTextChanged);

    // 连接选项控件信号
    connect(ui->checkBoxMatchCase, &QCheckBox::toggled, this, &FindReplaceWidget::onCaseSensitiveToggled);
    connect(ui->checkBoxMatchWholeWord, &QCheckBox::toggled, this, &FindReplaceWidget::onWholeWordToggled);
    connect(ui->checkBoxWrapAround, &QCheckBox::toggled, this, &FindReplaceWidget::onWrapAroundToggled);
    connect(ui->checkBoxBackwardsDirection, &QCheckBox::toggled, this, &FindReplaceWidget::onBackwardsToggled);

    ui->comboFind->setCompleter(nullptr);
    ui->comboReplace->setCompleter(nullptr);
    ui->checkBoxWrapAround->setChecked(true);
}

FindReplaceWidget::~FindReplaceWidget()
{
    delete ui;
}

void FindReplaceWidget::setMultiEditor(MultiEditor *multiEditor)
{
    m_multiEditor = multiEditor;
}

void FindReplaceWidget::setFindReplaceManager(FindReplaceManager *manager)
{
    m_manager = manager;
}

void FindReplaceWidget::showWithInitialText(const QString &initialText)
{
    if (!initialText.isEmpty()) {
        ui->comboFind->setCurrentText(initialText);
    }
    show();
    setFocus();
}

void FindReplaceWidget::refreshHighlights()
{
    if (m_manager) {
        m_manager->refreshHighlights();
    }
}

void FindReplaceWidget::findNext()
{
    if (m_manager) {
        m_manager->findNext();
    }
}

void FindReplaceWidget::setFocus()
{
    ui->comboFind->setFocus();
    ui->comboFind->lineEdit()->selectAll();
}

void FindReplaceWidget::find()
{
    onFindClicked();
}

void FindReplaceWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    emit visibilityChanged(true);
}

void FindReplaceWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit visibilityChanged(false);
}

void FindReplaceWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (ui->comboFind->hasFocus()) {
            onFindClicked();
            return;
        } else if (ui->comboReplace->hasFocus()) {
            onReplaceClicked();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void FindReplaceWidget::onFindClicked()
{
    if (m_manager) {
        m_manager->find(ui->comboFind->currentText());
    }
}

void FindReplaceWidget::onReplaceClicked()
{
    if (m_manager) {
        m_manager->replace(ui->comboReplace->currentText());
    }
}

void FindReplaceWidget::onReplaceAllClicked()
{
    if (m_manager) {
        m_manager->replaceAll();
    }
}

void FindReplaceWidget::onCloseClicked()
{
    close();
}

void FindReplaceWidget::onFindTextChanged(const QString &text)
{
    if (m_manager) {
        m_manager->setFindText(text);
    }
}

void FindReplaceWidget::onReplaceTextChanged(const QString &text)
{
    if (m_manager) {
        m_manager->setReplaceText(text);
    }
}

void FindReplaceWidget::onCaseSensitiveToggled(bool checked)
{
    if (m_manager) {
        m_manager->setCaseSensitive(checked);
    }
}

void FindReplaceWidget::onWholeWordToggled(bool checked)
{
    if (m_manager) {
        m_manager->setWholeWord(checked);
    }
}

void FindReplaceWidget::onWrapAroundToggled(bool checked)
{
    if (m_manager) {
        m_manager->setWrapAround(checked);
    }
}

void FindReplaceWidget::onBackwardsToggled(bool checked)
{
    if (m_manager) {
        m_manager->setBackwards(checked);
    }
}
