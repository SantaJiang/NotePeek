#include "find/findreplacewidget.h"
#include "ui_findreplacewidget.h"
#include "find/findreplacemanager.h"
#include "themecolors.h"
#include "widgets/iconfont.h"
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QToolButton>
#include <QTimer>

FindReplaceWidget::FindReplaceWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FindReplaceWidget)
    , m_manager(nullptr)
{
    ui->setupUi(this);

    // Draw a chrome separator line along the top edge so the bar reads as part
    // of the editor frame, matching the editor top bar's borders.
    setAttribute(Qt::WA_StyledBackground, true);
    applyChromeTheme();

    const QString &iconFontFamily = IconFont::fontAwesomeFamily();
    if (!iconFontFamily.isEmpty()) {
        ui->buttonClose->setFont(QFont(iconFontFamily));
    }
    ui->buttonClose->setText(QChar(0xf00d));
    ui->buttonClose->setToolTip(tr("Close find and replace"));
    ui->buttonClose->setAccessibleName(tr("Close find and replace"));
    ui->buttonFind->setAccessibleName(tr("Find next"));
    ui->buttonReplace->setAccessibleName(tr("Replace current match"));
    ui->buttonReplaceAll->setAccessibleName(tr("Replace all matches"));
    ui->buttonAdvanced->setAccessibleName(tr("Advanced search"));
    ui->comboFind->setAccessibleName(tr("Search text"));
    ui->comboReplace->setAccessibleName(tr("Replacement text"));

    connect(ui->buttonFind, &QToolButton::clicked, this, &FindReplaceWidget::onFindClicked);
    connect(ui->buttonReplace, &QToolButton::clicked, this, &FindReplaceWidget::onReplaceClicked);
    connect(ui->buttonReplaceAll, &QToolButton::clicked, this, &FindReplaceWidget::onReplaceAllClicked);
    connect(ui->buttonAdvanced, &QToolButton::clicked, this, &FindReplaceWidget::advancedSearchRequested);
    connect(ui->comboFind->lineEdit(), &QLineEdit::textChanged, this, &FindReplaceWidget::onFindTextChanged);
    connect(ui->comboReplace->lineEdit(), &QLineEdit::textChanged, this, &FindReplaceWidget::onReplaceTextChanged);

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

void FindReplaceWidget::applyChromeTheme()
{
    using namespace ThemeColors::EditorTopBar;

    const QString style = QStringLiteral(
        "FindReplaceWidget {"
        "  background-color: %5;"
        "  border-top: 1px solid %1;"
        "}"
        "QToolButton#buttonFind,"
        "QToolButton#buttonReplace,"
        "QToolButton#buttonReplaceAll,"
        "QToolButton#buttonAdvanced,"
        "QToolButton#buttonClose {"
        "  background-color: transparent;"
        "  color: %2;"
        "  border: 0;"
        "  padding: 0;"
        "  margin: 0;"
        "  border-radius: 4px;"
        "}"
        "QToolButton#buttonFind,"
        "QToolButton#buttonReplace,"
        "QToolButton#buttonReplaceAll,"
        "QToolButton#buttonAdvanced {"
        "  padding: 0 8px;"
        "}"
        "QToolButton#buttonFind:hover,"
        "QToolButton#buttonReplace:hover,"
        "QToolButton#buttonReplaceAll:hover,"
        "QToolButton#buttonAdvanced:hover,"
        "QToolButton#buttonClose:hover {"
        "  background-color: %3;"
        "}"
        "QToolButton#buttonFind:pressed,"
        "QToolButton#buttonReplace:pressed,"
        "QToolButton#buttonReplaceAll:pressed,"
        "QToolButton#buttonAdvanced:pressed,"
        "QToolButton#buttonClose:pressed {"
        "  background-color: %4;"
        "}"
        "QLineEdit,"
        "QComboBox,"
        "QComboBox QLineEdit,"
        "QCheckBox::indicator {"
        "  background-color: %6;"
        "}"
    ).arg(border().name(QColor::HexRgb),
          ThemeColors::Editor::foreground().name(QColor::HexRgb),
          hoverBg().name(QColor::HexRgb),
          pressedBg().name(QColor::HexRgb),
          background().name(QColor::HexRgb),
          ThemeColors::FileTree::background().name(QColor::HexRgb));

    if (styleSheet() != style)
    {
        setStyleSheet(style);
    }
}

void FindReplaceWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::PaletteChange)
    {
        QTimer::singleShot(0, this, &FindReplaceWidget::applyChromeTheme);
    }
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
        } else {
            onFindClicked();
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
