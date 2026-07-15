#include "preferencesdialog.h"

#include "apppreferences.h"
#include "themecolors.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr int kMinFontSize = 6;
constexpr int kMaxFontSize = 288;
constexpr int kDialogWidth = 520;
constexpr int kLabelWidth = 150;
constexpr int kCommonFontSizes[] = {9, 10, 11, 12, 13, 14, 18, 24, 36, 48, 64, 72, 96, 144, 288};

void polishDialogPalette(QWidget *widget)
{
    using namespace ThemeColors::Preferences;

    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, windowBg());
    palette.setColor(QPalette::WindowText, text());
    palette.setColor(QPalette::Base, inputBg());
    palette.setColor(QPalette::AlternateBase, panelBg());
    palette.setColor(QPalette::Text, text());
    palette.setColor(QPalette::Button, panelBg());
    palette.setColor(QPalette::ButtonText, text());
    palette.setColor(QPalette::Highlight, accent());
    palette.setColor(QPalette::HighlightedText, accentText());
    widget->setPalette(palette);
}

QLabel *makeSectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

QHBoxLayout *makeSettingsRow(const QString &labelText, QWidget *control, QWidget *parent)
{
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(16);

    auto *label = new QLabel(labelText, parent);
    label->setObjectName(QStringLiteral("rowLabel"));
    label->setFixedWidth(kLabelWidth);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    control->setParent(parent);
    row->addWidget(label);
    row->addWidget(control, 1);
    return row;
}

QFrame *makeSection(const QString &title, QWidget *parent)
{
    auto *section = new QFrame(parent);
    section->setObjectName(QStringLiteral("settingsSection"));
    section->setFrameShape(QFrame::NoFrame);
    section->setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeSectionTitle(title, section));

    return section;
}

void configureFontCombo(QFontComboBox *combo)
{
    combo->setEditable(false);
    combo->setInsertPolicy(QComboBox::NoInsert);
}

void configureFontSizeCombo(QComboBox *combo)
{
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->setFixedWidth(92);
    combo->lineEdit()->setValidator(new QIntValidator(kMinFontSize, kMaxFontSize, combo));

    for (int size : kCommonFontSizes)
    {
        combo->addItem(QString::number(size), size);
    }
}

int currentFontSize(QComboBox *combo)
{
    bool ok = false;
    const int pointSize = combo->currentText().trimmed().toInt(&ok);
    if (ok)
    {
        return pointSize;
    }

    const QVariant data = combo->currentData();
    return data.isValid() ? data.toInt() : AppPreferences::defaultFontSize();
}

void setCurrentFontSize(QComboBox *combo, int pointSize)
{
    const int index = combo->findData(pointSize);
    if (index >= 0)
    {
        combo->setCurrentIndex(index);
        return;
    }

    combo->setEditText(QString::number(pointSize));
}

// A family picker paired with a point-size dropdown, laid out horizontally so
// the two fonts line up neatly in the form.
QWidget *makeFontRow(QFontComboBox *combo, QComboBox *size, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    combo->setParent(row);
    size->setParent(row);

    layout->addWidget(combo, 1);
    layout->addWidget(size);
    return row;
}

QString preferencesStyleSheet()
{
    using namespace ThemeColors::Preferences;

    return QStringLiteral(
        "QDialog {"
        "  background-color: %1;"
        "  color: %5;"
        "}"
        "QLabel#sectionTitle {"
        "  color: %5;"
        "  font-weight: 600;"
        "}"
        "QLabel#rowLabel {"
        "  color: %6;"
        "}"
        "QFrame#settingsSection {"
        "  background-color: %2;"
        "  border: 1px solid %4;"
        "  border-radius: 6px;"
        "}"
        "QCheckBox {"
        "  color: %5;"
        "  spacing: 8px;"
        "}"
        "QPushButton {"
        "  background-color: %3;"
        "  color: %5;"
        "  border: 1px solid %4;"
        "  border-radius: 4px;"
        "  min-width: 72px;"
        "  padding: 5px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %7;"
        "}"
        "QPushButton:pressed {"
        "  background-color: %8;"
        "}"
        "QPushButton:default {"
        "  background-color: %9;"
        "  border-color: %9;"
        "  color: %10;"
        "}"
        "QPushButton:default:hover {"
        "  background-color: %11;"
        "  border-color: %11;"
        "}"
        "QAbstractItemView {"
        "  background-color: %2;"
        "  color: %5;"
        "  border: 1px solid %4;"
        "  outline: 0;"
        "  selection-background-color: %7;"
        "  selection-color: %5;"
        "}"
        "QAbstractItemView::item {"
        "  min-height: 22px;"
        "  padding: 2px 8px;"
        "}"
        "QAbstractItemView::item:hover {"
        "  background-color: %7;"
        "}")
        .arg(windowBg().name(QColor::HexRgb),
             panelBg().name(QColor::HexRgb),
             inputBg().name(QColor::HexRgb),
             border().name(QColor::HexRgb),
             text().name(QColor::HexRgb),
             mutedText().name(QColor::HexRgb),
             hoverBg().name(QColor::HexRgb),
             pressedBg().name(QColor::HexRgb),
             accent().name(QColor::HexRgb),
             accentText().name(QColor::HexRgb),
             accentPressed().name(QColor::HexRgb));
}

} // namespace

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
    , m_textFontCombo(new QFontComboBox(this))
    , m_textFontSize(new QComboBox(this))
    , m_codeFontCombo(new QFontComboBox(this))
    , m_codeFontSize(new QComboBox(this))
    , m_showFileTree(new QCheckBox(tr("Show file tree on startup"), this))
    , m_markdownSidePreview(new QCheckBox(tr("Open with side preview"), this))
    , m_codeFolding(new QCheckBox(tr("Show folding controls"), this))
    , m_bracketAssistance(new QCheckBox(tr("Match and auto-close brackets"), this))
    , m_smartIndentation(new QCheckBox(tr("Indent guides and smart Tab/Backspace"), this))
    , m_documentCompletion(new QCheckBox(tr("Suggest words from the current document"), this))
{
    setWindowTitle(tr("Preferences"));
    setModal(true);
    polishDialogPalette(this);
    setStyleSheet(preferencesStyleSheet());

    // The code font must be monospaced; restrict the picker accordingly.
    configureFontCombo(m_textFontCombo);
    configureFontCombo(m_codeFontCombo);
    m_codeFontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    configureFontSizeCombo(m_textFontSize);
    configureFontSizeCombo(m_codeFontSize);

    auto *fontsSection = makeSection(tr("Editor Fonts"), this);
    auto *fontsLayout = qobject_cast<QVBoxLayout *>(fontsSection->layout());
    fontsLayout->addLayout(makeSettingsRow(tr("Text font"), makeFontRow(m_textFontCombo, m_textFontSize, fontsSection), fontsSection));
    fontsLayout->addLayout(makeSettingsRow(tr("Code font"), makeFontRow(m_codeFontCombo, m_codeFontSize, fontsSection), fontsSection));

    auto *generalSection = makeSection(tr("General"), this);
    auto *generalLayout = qobject_cast<QVBoxLayout *>(generalSection->layout());
    generalLayout->addLayout(makeSettingsRow(tr("Startup"), m_showFileTree, generalSection));
    generalLayout->addLayout(makeSettingsRow(tr("Markdown"), m_markdownSidePreview, generalSection));

    auto *codeSection = makeSection(tr("Code Editing"), this);
    auto *codeLayout = qobject_cast<QVBoxLayout *>(codeSection->layout());
    codeLayout->addLayout(makeSettingsRow(tr("Folding"), m_codeFolding, codeSection));
    codeLayout->addLayout(makeSettingsRow(tr("Brackets"), m_bracketAssistance, codeSection));
    codeLayout->addLayout(makeSettingsRow(tr("Indentation"), m_smartIndentation, codeSection));
    codeLayout->addLayout(makeSettingsRow(tr("Completion"), m_documentCompletion, codeSection));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                             | QDialogButtonBox::Apply
                                             | QDialogButtonBox::Cancel,
                                         this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 18);
    layout->setSpacing(14);
    layout->addWidget(fontsSection);
    layout->addWidget(generalSection);
    layout->addWidget(codeSection);
    layout->addStretch();
    layout->addWidget(buttons);

    // Lock the dialog to a fixed size so it cannot be resized/maximised.
    setFixedSize(kDialogWidth, sizeHint().height());

    loadFromPreferences();

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        commit();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &PreferencesDialog::commit);
}

void PreferencesDialog::loadFromPreferences()
{
    const AppPreferences &prefs = AppPreferences::instance();

    m_textFontCombo->setCurrentFont(QFont(prefs.textFontFamily()));
    setCurrentFontSize(m_textFontSize, prefs.textFontSize());
    m_codeFontCombo->setCurrentFont(QFont(prefs.codeFontFamily()));
    setCurrentFontSize(m_codeFontSize, prefs.codeFontSize());
    m_showFileTree->setChecked(prefs.showFileTreeOnStartup());
    m_markdownSidePreview->setChecked(prefs.markdownDefaultPreview());
    m_codeFolding->setChecked(prefs.codeFoldingEnabled());
    m_bracketAssistance->setChecked(prefs.bracketAssistanceEnabled());
    m_smartIndentation->setChecked(prefs.smartIndentationEnabled());
    m_documentCompletion->setChecked(prefs.documentCompletionEnabled());
}

void PreferencesDialog::commit()
{
    AppPreferences &prefs = AppPreferences::instance();

    prefs.setTextFont(m_textFontCombo->currentFont().family(), currentFontSize(m_textFontSize));
    prefs.setCodeFont(m_codeFontCombo->currentFont().family(), currentFontSize(m_codeFontSize));
    prefs.setShowFileTreeOnStartup(m_showFileTree->isChecked());
    prefs.setMarkdownDefaultPreview(m_markdownSidePreview->isChecked());
    prefs.setCodeFoldingEnabled(m_codeFolding->isChecked());
    prefs.setBracketAssistanceEnabled(m_bracketAssistance->isChecked());
    prefs.setSmartIndentationEnabled(m_smartIndentation->isChecked());
    prefs.setDocumentCompletionEnabled(m_documentCompletion->isChecked());

    prefs.save();

    // Reflect any validation/normalisation the model applied (e.g. an
    // unavailable family snapping back to a default).
    loadFromPreferences();

    emit applied();
}
