#include "preferencesdialog.h"

#include "apppreferences.h"
#include "themecolors.h"
#include <NotePeek/syntaxtheme.h>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPalette>
#include <QPushButton>
#include <QStackedWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace {

constexpr int kMinFontSize = 6;
constexpr int kMaxFontSize = 288;
constexpr int kDialogWidth = 720;
constexpr int kSidebarWidth = 180;
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

void configureFontCombo(QComboBox *combo)
{
    combo->setEditable(false);
    combo->setInsertPolicy(QComboBox::NoInsert);
}

// Populate the code-font picker with every installed monospace family.
// Unlike QFontComboBox::MonospacedFonts (which only honours the font's
// fixed-pitch bit), this also includes genuinely-monospace families whose
// metadata is wrong, via AppPreferences::isMonospaceFamily.  Each entry is
// rendered in its own family so the dropdown doubles as a preview.
void populateCodeFontCombo(QComboBox *combo)
{
    combo->clear();
    for (const QString &family : QFontDatabase::families())
    {
        if (!AppPreferences::isMonospaceFamily(family))
        {
            continue;
        }
        combo->addItem(family);
        combo->setItemData(combo->count() - 1, QVariant::fromValue(QFont(family)), Qt::FontRole);
    }
}

// Mirror the relevant slice of QFontComboBox's API so the code-font picker
// stays drop-in compatible with the text-font one at the call sites.
void setComboCurrentFont(QComboBox *combo, const QFont &font)
{
    const int index = combo->findText(font.family());
    if (index >= 0)
    {
        combo->setCurrentIndex(index);
    }
}

QFont comboCurrentFont(const QComboBox *combo)
{
    return QFont(combo->currentText());
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
// the two fonts line up neatly in the form.  Accepts a plain QComboBox so the
// same row layout serves the QFontComboBox text picker and the custom code
// picker.
QWidget *makeFontRow(QComboBox *combo, QComboBox *size, QWidget *parent)
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
        "  background-color: transparent;"
        "  border: none;"
        "}"
        "QListWidget#sectionList {"
        "  background-color: %2;"
        "  color: %5;"
        "  border: 1px solid %4;"
        "  border-radius: 6px;"
        "  outline: 0;"
        "  padding: 4px;"
        "}"
        "QListWidget#sectionList::item {"
        "  padding: 8px 12px;"
        "  border-radius: 4px;"
        "}"
        "QListWidget#sectionList::item:hover {"
        "  background-color: %7;"
        "}"
        "QListWidget#sectionList::item:selected {"
        "  background-color: %9;"
        "  color: %10;"
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
    , m_codeFontCombo(new QComboBox(this))
    , m_codeFontSize(new QComboBox(this))
    , m_syntaxThemeCombo(new QComboBox(this))
    , m_showFileTree(new QCheckBox(tr("Show file tree on startup"), this))
    , m_showStatusBar(new QCheckBox(tr("Show bottom status bar"), this))
    , m_markdownSidePreview(new QCheckBox(tr("Open with side preview"), this))
    , m_untitledModeCombo(new QComboBox(this))
    , m_overtypeMode(new QComboBox(this))
    , m_codeFolding(new QCheckBox(tr("Show folding controls"), this))
    , m_bracketAssistance(new QCheckBox(tr("Match and auto-close brackets"), this))
    , m_smartIndentation(new QCheckBox(tr("Indent guides and smart Tab/Backspace"), this))
    , m_documentCompletion(new QCheckBox(tr("Suggest words from the current document"), this))
    , m_semanticHighlighting(new QCheckBox(tr("Add function, type, member and variable colors"), this))
    , m_languageCombo(new QComboBox(this))
    , m_sectionList(new QListWidget(this))
    , m_sectionStack(new QStackedWidget(this))
{
    setWindowTitle(tr("Preferences"));
    setModal(true);
    m_showStatusBar->setObjectName(QStringLiteral("showStatusBarCheckBox"));
    polishDialogPalette(this);
    setStyleSheet(preferencesStyleSheet());

    // The text font picker shows every family; the code font picker is
    // restricted to monospaced families, including ones whose fixed-pitch
    // metadata is wrong (detected by glyph width) so e.g. "IoskeleyMono
    // Nerd Font" still appears.
    configureFontCombo(m_textFontCombo);
    configureFontCombo(m_codeFontCombo);
    populateCodeFontCombo(m_codeFontCombo);
    configureFontSizeCombo(m_textFontSize);
    configureFontSizeCombo(m_codeFontSize);

    // Sidebar navigation list.  Each entry maps, by row index, to a page in
    // the stacked widget on the right.
    m_sectionList->setObjectName(QStringLiteral("sectionList"));
    m_sectionList->setFixedWidth(kSidebarWidth);
    m_sectionList->setUniformItemSizes(true);
    m_sectionList->setFocusPolicy(Qt::StrongFocus);
    m_sectionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *editorSection = makeSection(tr("Editor"), this);
    auto *editorLayout = qobject_cast<QVBoxLayout *>(editorSection->layout());
    editorLayout->addLayout(makeSettingsRow(tr("Text font"), makeFontRow(m_textFontCombo, m_textFontSize, editorSection), editorSection));
    editorLayout->addLayout(makeSettingsRow(tr("Code font"), makeFontRow(m_codeFontCombo, m_codeFontSize, editorSection), editorSection));

    m_syntaxThemeCombo->addItem(tr("Automatic (system appearance)"), QStringLiteral("auto"));
    m_syntaxThemeCombo->setObjectName(QStringLiteral("syntaxThemeCombo"));
    const QVector<NotePeek::SyntaxTheme> syntaxThemes = NotePeek::SyntaxThemeCatalog::builtInThemes();
    for (const NotePeek::SyntaxTheme &theme : syntaxThemes)
        m_syntaxThemeCombo->addItem(theme.name, theme.id);
    m_syntaxThemeCombo->setToolTip(tr("Applies immediately to syntax editors."));
    editorLayout->addLayout(makeSettingsRow(tr("Syntax theme"), m_syntaxThemeCombo, editorSection));
    editorLayout->addStretch();

    // Language names stay untranslated so each entry is readable in the
    // language it selects. Takes effect after a restart.
    m_languageCombo->addItem(tr("System default"), QString());
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_languageCombo->addItem(QStringLiteral("简体中文"), QStringLiteral("zh_CN"));
    m_languageCombo->setToolTip(tr("Takes effect the next time NotePeek starts."));

    // Presentation of the "untitled" document, which has no file extension to
    // infer a mode from.
    m_untitledModeCombo->addItem(tr("Text (wrap lines, no line numbers)"), false);
    m_untitledModeCombo->addItem(tr("Code (line numbers, no wrapping)"), true);
    m_untitledModeCombo->setToolTip(tr("Applies to untitled documents that have not been saved yet."));

    // Overtype mode selection (the keyboard INS/OVR toggle).  Insert is the
    // normal typing behaviour; Overwrite replaces the character ahead of the
    // caret instead.
    m_overtypeMode->addItem(tr("Insert (INS)"), false);
    m_overtypeMode->addItem(tr("Overwrite (OVR)"), true);
    m_overtypeMode->setToolTip(tr("Controls whether typing inserts or replaces the character ahead of the caret."));

    auto *generalSection = makeSection(tr("General"), this);
    auto *generalLayout = qobject_cast<QVBoxLayout *>(generalSection->layout());
    generalLayout->addLayout(makeSettingsRow(tr("Language"), m_languageCombo, generalSection));
    generalLayout->addLayout(makeSettingsRow(tr("Interface"), m_showStatusBar, generalSection));
    generalLayout->addLayout(makeSettingsRow(tr("Startup"), m_showFileTree, generalSection));
    generalLayout->addLayout(makeSettingsRow(tr("New document"), m_untitledModeCombo, generalSection));
    generalLayout->addLayout(makeSettingsRow(tr("Typing"), m_overtypeMode, generalSection));
    generalLayout->addLayout(makeSettingsRow(tr("Markdown"), m_markdownSidePreview, generalSection));
    generalLayout->addStretch();

    auto *codeSection = makeSection(tr("Code Editing"), this);
    auto *codeLayout = qobject_cast<QVBoxLayout *>(codeSection->layout());
    codeLayout->addLayout(makeSettingsRow(tr("Folding"), m_codeFolding, codeSection));
    codeLayout->addLayout(makeSettingsRow(tr("Brackets"), m_bracketAssistance, codeSection));
    codeLayout->addLayout(makeSettingsRow(tr("Indentation"), m_smartIndentation, codeSection));
    codeLayout->addLayout(makeSettingsRow(tr("Completion"), m_documentCompletion, codeSection));
    codeLayout->addLayout(makeSettingsRow(tr("Semantic highlighting"), m_semanticHighlighting, codeSection));
    codeLayout->addStretch();

    // Wire each section into the right pane as a stacked page, and add a
    // matching sidebar entry.  The page index returned by addWidget is stored
    // on the list item (Qt::UserRole) so each entry stays bound to its page
    // regardless of the order in which pages and entries are added.
    const int editorIndex = m_sectionStack->addWidget(editorSection);
    const int generalIndex = m_sectionStack->addWidget(generalSection);
    const int codeIndex = m_sectionStack->addWidget(codeSection);

    auto *editorItem = new QListWidgetItem(tr("Editor"), m_sectionList);
    editorItem->setData(Qt::UserRole, editorIndex);
    auto *generalItem = new QListWidgetItem(tr("General"), m_sectionList);
    generalItem->setData(Qt::UserRole, generalIndex);
    auto *codeItem = new QListWidgetItem(tr("Code Editing"), m_sectionList);
    codeItem->setData(Qt::UserRole, codeIndex);

    connect(m_sectionList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;
        const QVariant data = m_sectionList->item(row)->data(Qt::UserRole);
        m_sectionStack->setCurrentIndex(data.toInt());
    });
    // Show the first page deterministically; setCurrentRow then highlights the
    // matching sidebar entry (and the handler above keeps them in sync for
    // every subsequent user-driven change).
    m_sectionStack->setCurrentIndex(0);
    m_sectionList->setCurrentRow(0);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                             | QDialogButtonBox::Apply
                                             | QDialogButtonBox::Cancel,
                                         this);

    // Two-pane body: left sidebar + right stacked content.  The buttons sit
    // beneath the whole body as before.
    auto *bodyLayout = new QHBoxLayout;
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(14);
    bodyLayout->addWidget(m_sectionList);
    bodyLayout->addWidget(m_sectionStack, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 18);
    layout->setSpacing(14);
    layout->addLayout(bodyLayout);
    layout->addStretch();
    layout->addWidget(buttons);

    // Keep the compact default while allowing larger fonts and translations
    // to request more space.
    setMinimumWidth(kDialogWidth);
    resize(kDialogWidth, sizeHint().height());

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
    setComboCurrentFont(m_codeFontCombo, QFont(prefs.codeFontFamily()));
    setCurrentFontSize(m_codeFontSize, prefs.codeFontSize());
    const int themeIndex = m_syntaxThemeCombo->findData(prefs.syntaxThemeId());
    m_syntaxThemeCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    m_showFileTree->setChecked(prefs.showFileTreeOnStartup());
    m_showStatusBar->setChecked(prefs.showStatusBar());
    m_markdownSidePreview->setChecked(prefs.markdownDefaultPreview());
    const int untitledModeIndex = m_untitledModeCombo->findData(prefs.untitledUsesCodeMode());
    m_untitledModeCombo->setCurrentIndex(untitledModeIndex >= 0 ? untitledModeIndex : 0);
    const int overtypeIndex = m_overtypeMode->findData(prefs.overwriteMode());
    m_overtypeMode->setCurrentIndex(overtypeIndex >= 0 ? overtypeIndex : 0);
    m_codeFolding->setChecked(prefs.codeFoldingEnabled());
    m_bracketAssistance->setChecked(prefs.bracketAssistanceEnabled());
    m_smartIndentation->setChecked(prefs.smartIndentationEnabled());
    m_documentCompletion->setChecked(prefs.documentCompletionEnabled());
    m_semanticHighlighting->setChecked(prefs.semanticHighlightingEnabled());

    const int languageIndex = m_languageCombo->findData(prefs.language());
    m_languageCombo->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);
}

void PreferencesDialog::commit()
{
    AppPreferences &prefs = AppPreferences::instance();

    prefs.setTextFont(m_textFontCombo->currentFont().family(), currentFontSize(m_textFontSize));
    prefs.setCodeFont(comboCurrentFont(m_codeFontCombo).family(), currentFontSize(m_codeFontSize));
    prefs.setSyntaxThemeId(m_syntaxThemeCombo->currentData().toString());
    prefs.setShowFileTreeOnStartup(m_showFileTree->isChecked());
    prefs.setShowStatusBar(m_showStatusBar->isChecked());
    prefs.setMarkdownDefaultPreview(m_markdownSidePreview->isChecked());
    prefs.setUntitledUsesCodeMode(m_untitledModeCombo->currentData().toBool());
    prefs.setOverwriteMode(m_overtypeMode->currentData().toBool());
    prefs.setCodeFoldingEnabled(m_codeFolding->isChecked());
    prefs.setBracketAssistanceEnabled(m_bracketAssistance->isChecked());
    prefs.setSmartIndentationEnabled(m_smartIndentation->isChecked());
    prefs.setDocumentCompletionEnabled(m_documentCompletion->isChecked());
    prefs.setSemanticHighlightingEnabled(m_semanticHighlighting->isChecked());
    prefs.setLanguage(m_languageCombo->currentData().toString());

    prefs.save();

    // Reflect any validation/normalisation the model applied (e.g. an
    // unavailable family snapping back to a default).
    loadFromPreferences();

    emit applied();
}
