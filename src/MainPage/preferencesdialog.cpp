#include "preferencesdialog.h"

#include "apppreferences.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

constexpr int kMinFontSize = 6;
constexpr int kMaxFontSize = 72;

// A family picker paired with a point-size spin box, laid out horizontally so
// the two fonts line up neatly in the form.
QWidget *makeFontRow(QFontComboBox *combo, QSpinBox *size, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    combo->setParent(row);
    size->setParent(row);
    size->setRange(kMinFontSize, kMaxFontSize);
    size->setSuffix(QObject::tr(" pt"));

    layout->addWidget(combo, 1);
    layout->addWidget(size);
    return row;
}

} // namespace

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
    , m_textFontCombo(new QFontComboBox(this))
    , m_textFontSize(new QSpinBox(this))
    , m_codeFontCombo(new QFontComboBox(this))
    , m_codeFontSize(new QSpinBox(this))
    , m_showFileTree(new QCheckBox(tr("Show file tree on startup"), this))
    , m_markdownDefaultView(new QComboBox(this))
{
    setWindowTitle(tr("Preferences"));
    setModal(true);

    // The code font must be monospaced; restrict the picker accordingly.
    m_codeFontCombo->setFontFilters(QFontComboBox::MonospacedFonts);

    m_markdownDefaultView->addItem(tr("Source"));
    m_markdownDefaultView->addItem(tr("Preview"));

    auto *fontsGroup = new QGroupBox(tr("Editor Fonts"), this);
    auto *fontsForm = new QFormLayout(fontsGroup);
    fontsForm->setLabelAlignment(Qt::AlignLeft);
    fontsForm->addRow(tr("Text font:"), makeFontRow(m_textFontCombo, m_textFontSize, fontsGroup));
    fontsForm->addRow(tr("Code font:"), makeFontRow(m_codeFontCombo, m_codeFontSize, fontsGroup));

    auto *generalGroup = new QGroupBox(tr("General"), this);
    auto *generalForm = new QFormLayout(generalGroup);
    generalForm->setLabelAlignment(Qt::AlignLeft);
    generalForm->addRow(QString(), m_showFileTree);
    generalForm->addRow(tr("Markdown default view:"), m_markdownDefaultView);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                             | QDialogButtonBox::Apply
                                             | QDialogButtonBox::Cancel,
                                         this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);
    layout->addWidget(fontsGroup);
    layout->addWidget(generalGroup);
    layout->addStretch();
    layout->addWidget(buttons);

    setMinimumWidth(420);

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
    m_textFontSize->setValue(prefs.textFontSize());
    m_codeFontCombo->setCurrentFont(QFont(prefs.codeFontFamily()));
    m_codeFontSize->setValue(prefs.codeFontSize());
    m_showFileTree->setChecked(prefs.showFileTreeOnStartup());
    m_markdownDefaultView->setCurrentIndex(prefs.markdownDefaultPreview() ? 1 : 0);
}

void PreferencesDialog::commit()
{
    AppPreferences &prefs = AppPreferences::instance();

    prefs.setTextFont(m_textFontCombo->currentFont().family(), m_textFontSize->value());
    prefs.setCodeFont(m_codeFontCombo->currentFont().family(), m_codeFontSize->value());
    prefs.setShowFileTreeOnStartup(m_showFileTree->isChecked());
    prefs.setMarkdownDefaultPreview(m_markdownDefaultView->currentIndex() == 1);

    prefs.save();

    // Reflect any validation/normalisation the model applied (e.g. an
    // unavailable family snapping back to a default).
    loadFromPreferences();

    emit applied();
}
