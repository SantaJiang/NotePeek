#include "preferencesdialog.h"

#include "apppreferences.h"
#include "themecolors.h"
#include <NotePeek/syntaxtheme.h>

#include <QAbstractItemView>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyleHints>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>

namespace {

constexpr int kMinFontSize = 6;
constexpr int kMaxFontSize = 288;
constexpr int kDialogWidth = 700;
constexpr int kDialogHeight = 320;
constexpr int kSidebarWidth = 190;
constexpr int kLabelWidth = 150;
constexpr int kContentMargin = 20;
constexpr int kRowSpacing = 12;
// The bold CJK section-title glyphs have a visible left side bearing on the
// bundled/platform fonts. Shift the title widget left while keeping every row
// on the original form grid so their painted text edges align optically.
constexpr int kTitleOpticalOffset = 4;
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
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

QHBoxLayout *makeSettingsRow(const QString &labelText, QWidget *control, QWidget *parent)
{
    auto *row = new QHBoxLayout;
    row->setContentsMargins(kTitleOpticalOffset, 0, 0, 0);
    row->setSpacing(kRowSpacing);

    auto *label = new QLabel(labelText, parent);
    label->setObjectName(QStringLiteral("rowLabel"));
    label->setFixedWidth(kLabelWidth);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    control->setParent(parent);
    row->addWidget(label);
    row->addWidget(control, 1);
    return row;
}

QWidget *makeSettingsRowWidget(const QString &labelText, QWidget *control, QWidget *parent)
{
    auto *rowWidget = new QWidget(parent);
    rowWidget->setLayout(makeSettingsRow(labelText, control, rowWidget));
    return rowWidget;
}

QFrame *makeSection(const QString &title, QWidget *parent)
{
    auto *section = new QFrame(parent);
    section->setObjectName(QStringLiteral("settingsSection"));
    section->setFrameShape(QFrame::NoFrame);
    section->setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(kContentMargin - kTitleOpticalOffset,
                               16, kContentMargin, 16);
    layout->setSpacing(8);
    layout->addWidget(makeSectionTitle(title, section));

    return section;
}

QScrollArea *makeScrollablePage(QWidget *content, QWidget *parent)
{
    auto *scrollArea = new QScrollArea(parent);
    scrollArea->setObjectName(QStringLiteral("settingsScrollArea"));
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);
    return scrollArea;
}

void configureFontCombo(QComboBox *combo)
{
    combo->setEditable(false);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
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
        "  font-weight: bold;"
        "  font-size: 14px;"
        "  padding-bottom: 2px;"
        "}"
        "QLabel#rowLabel {"
        "  color: %6;"
        "}"
        "QFrame#settingsSection {"
        "  background-color: %1;"
        "  border: none;"
        "}"
        "QWidget#sidebar {"
        "  background-color: %2;"
        "  border-right: 1px solid %4;"
        "}"
        "QLineEdit#settingsSearch {"
        "  background-color: %3;"
        "  color: %5;"
        "  border: 1px solid %4;"
        "  border-radius: 5px;"
        "  padding: 5px 8px;"
        "}"
        "QLineEdit#settingsSearch:focus {"
        "  border-color: %9;"
        "}"
        "QScrollArea#settingsScrollArea {"
        "  background-color: %1;"
        "  border: none;"
        "}"
        "QScrollArea#settingsScrollArea > QWidget > QWidget {"
        "  background-color: %1;"
        "}"
        "QListWidget#sectionList {"
        "  background-color: %2;"
        "  color: %5;"
        "  border: none;"
        "  outline: 0;"
        "  padding: 0;"
        "}"
        "QListWidget#sectionList::item {"
        "  padding: 8px 10px;"
        "  margin: 1px 0;"
        "  border-radius: 6px;"
        "}"
        "QListWidget#sectionList::item:hover {"
        "  background-color: %7;"
        "}"
        "QListWidget#sectionList::item:selected {"
        "  background-color: %7;"
        "  color: %5;"
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
    , m_darkSyntaxThemeCombo(new QComboBox(this))
    , m_lightSyntaxThemeCombo(new QComboBox(this))
    , m_importSyntaxThemeButton(new QPushButton(tr("Import theme..."), this))
    , m_openSyntaxThemeFolderButton(new QPushButton(tr("Open theme folder"), this))
    , m_showFileTree(new QCheckBox(tr("Show file tree on startup"), this))
    , m_showStatusBar(new QCheckBox(tr("Show bottom status bar"), this))
    , m_gitIntegrationEnabled(new QCheckBox(tr("Enable Git integration"), this))
    , m_markdownSidePreview(new QCheckBox(tr("Open with side preview"), this))
    , m_untitledModeCombo(new QComboBox(this))
    , m_overtypeMode(new QComboBox(this))
    , m_codeFolding(new QCheckBox(tr("Show folding controls"), this))
    , m_bracketAssistance(new QCheckBox(tr("Match and auto-close brackets"), this))
    , m_smartIndentation(new QCheckBox(tr("Indent guides and smart Tab/Backspace"), this))
    , m_documentCompletion(new QCheckBox(tr("Suggest words from the current document"), this))
    , m_semanticHighlighting(new QCheckBox(tr("Add function, type, member and variable colors"), this))
    , m_languageCombo(new QComboBox(this))
    , m_searchEdit(new QLineEdit(this))
    , m_sectionList(new QListWidget(this))
    , m_sectionStack(new QStackedWidget(this))
{
    setWindowTitle(tr("Preferences"));
    setModal(true);
    m_showStatusBar->setObjectName(QStringLiteral("showStatusBarCheckBox"));
    m_gitIntegrationEnabled->setObjectName(
        QStringLiteral("gitIntegrationEnabledCheckBox"));
    reapplyTheme();

    // The text font picker shows every family; the code font picker is
    // restricted to monospaced families, including ones whose fixed-pitch
    // metadata is wrong (detected by glyph width) so e.g. "IoskeleyMono
    // Nerd Font" still appears.
    configureFontCombo(m_textFontCombo);
    configureFontCombo(m_codeFontCombo);
    populateCodeFontCombo(m_codeFontCombo);
    configureFontSizeCombo(m_textFontSize);
    configureFontSizeCombo(m_codeFontSize);

    // Sidebar navigation. Search filters whole sections using the visible
    // category and setting labels, without changing any pending values.
    m_searchEdit->setObjectName(QStringLiteral("settingsSearch"));
    m_searchEdit->setPlaceholderText(tr("Search settings..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_sectionList->setObjectName(QStringLiteral("sectionList"));
    m_sectionList->setUniformItemSizes(true);
    m_sectionList->setFocusPolicy(Qt::StrongFocus);
    m_sectionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *editorSection = makeSection(tr("Editor"), this);
    auto *editorLayout = qobject_cast<QVBoxLayout *>(editorSection->layout());
    editorLayout->addLayout(makeSettingsRow(tr("Text font"), makeFontRow(m_textFontCombo, m_textFontSize, editorSection), editorSection));
    editorLayout->addLayout(makeSettingsRow(tr("Code font"), makeFontRow(m_codeFontCombo, m_codeFontSize, editorSection), editorSection));

    m_darkSyntaxThemeCombo->setObjectName(QStringLiteral("darkSyntaxThemeCombo"));
    m_lightSyntaxThemeCombo->setObjectName(QStringLiteral("lightSyntaxThemeCombo"));
    m_importSyntaxThemeButton->setObjectName(QStringLiteral("importSyntaxThemeButton"));
    m_openSyntaxThemeFolderButton->setObjectName(QStringLiteral("openSyntaxThemeFolderButton"));
    populateSyntaxThemeCombos();
    const QString themeToolTip = tr("Applies immediately to syntax editors.");
    m_darkSyntaxThemeCombo->setToolTip(themeToolTip);
    m_lightSyntaxThemeCombo->setToolTip(themeToolTip);
    m_darkSyntaxThemeRow = makeSettingsRowWidget(
        tr("Dark syntax theme"), m_darkSyntaxThemeCombo, editorSection);
    m_darkSyntaxThemeRow->setObjectName(QStringLiteral("darkSyntaxThemeRow"));
    m_lightSyntaxThemeRow = makeSettingsRowWidget(
        tr("Light syntax theme"), m_lightSyntaxThemeCombo, editorSection);
    m_lightSyntaxThemeRow->setObjectName(QStringLiteral("lightSyntaxThemeRow"));
    editorLayout->addWidget(m_darkSyntaxThemeRow);
    editorLayout->addWidget(m_lightSyntaxThemeRow);

    auto *themeActions = new QWidget(editorSection);
    auto *themeActionsLayout = new QHBoxLayout(themeActions);
    themeActionsLayout->setContentsMargins(0, 0, 0, 0);
    themeActionsLayout->setSpacing(8);
    themeActionsLayout->addWidget(m_importSyntaxThemeButton);
    themeActionsLayout->addWidget(m_openSyntaxThemeFolderButton);
    editorLayout->addLayout(makeSettingsRow(tr("Theme files"), themeActions, editorSection));
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

    auto *versionControlSection = makeSection(tr("Version Control"), this);
    auto *versionControlLayout =
        qobject_cast<QVBoxLayout *>(versionControlSection->layout());
    versionControlLayout->addLayout(
        makeSettingsRow(tr("Git"), m_gitIntegrationEnabled, versionControlSection));
    versionControlLayout->addStretch();

    // Wire each section into the right pane as a stacked page, and add a
    // matching sidebar entry.  The page index returned by addWidget is stored
    // on the list item (Qt::UserRole) so each entry stays bound to its page
    // regardless of the order in which pages and entries are added.
    const int editorIndex = m_sectionStack->addWidget(
        makeScrollablePage(editorSection, m_sectionStack));
    const int generalIndex = m_sectionStack->addWidget(
        makeScrollablePage(generalSection, m_sectionStack));
    const int codeIndex = m_sectionStack->addWidget(
        makeScrollablePage(codeSection, m_sectionStack));
    const int versionControlIndex = m_sectionStack->addWidget(
        makeScrollablePage(versionControlSection, m_sectionStack));

    auto *editorItem = new QListWidgetItem(tr("Editor"), m_sectionList);
    editorItem->setData(Qt::UserRole, editorIndex);
    auto *generalItem = new QListWidgetItem(tr("General"), m_sectionList);
    generalItem->setData(Qt::UserRole, generalIndex);
    auto *codeItem = new QListWidgetItem(tr("Code Editing"), m_sectionList);
    codeItem->setData(Qt::UserRole, codeIndex);
    auto *versionControlItem = new QListWidgetItem(tr("Version Control"), m_sectionList);
    versionControlItem->setData(Qt::UserRole, versionControlIndex);

    connect(m_sectionList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;
        QListWidgetItem *item = m_sectionList->item(row);
        if (item == nullptr || item->isHidden())
            return;
        const QVariant data = item->data(Qt::UserRole);
        m_sectionStack->setCurrentIndex(data.toInt());
    });
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &PreferencesDialog::filterSections);
    // Show the first page deterministically; setCurrentRow then highlights the
    // matching sidebar entry (and the handler above keeps them in sync for
    // every subsequent user-driven change).
    m_sectionStack->setCurrentIndex(0);
    m_sectionList->setCurrentRow(0);

    auto *sidebar = new QWidget(this);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(kSidebarWidth);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(12, 16, 12, 12);
    sidebarLayout->setSpacing(12);
    sidebarLayout->addWidget(m_searchEdit);
    sidebarLayout->addWidget(m_sectionList, 1);

    auto *rightPane = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(m_sectionStack, 1);

    auto *bodyLayout = new QHBoxLayout;
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(sidebar);
    bodyLayout->addWidget(rightPane, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(bodyLayout);

    // Keep the compact default while allowing larger fonts and translations
    // to request more space.
    setMinimumWidth(kDialogWidth);
    resize(kDialogWidth, kDialogHeight);

    loadFromPreferences();
    connectLiveUpdates();
    connect(m_importSyntaxThemeButton, &QPushButton::clicked,
            this, &PreferencesDialog::importSyntaxTheme);
    connect(m_openSyntaxThemeFolderButton, &QPushButton::clicked,
            this, &PreferencesDialog::openSyntaxThemeFolder);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this]() {
                scheduleThemeRefresh();
            });
#endif
}

void PreferencesDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::StyleChange)
    {
        scheduleThemeRefresh();
    }
}

void PreferencesDialog::filterSections(const QString &query)
{
    const QString needle = query.trimmed();
    QListWidgetItem *firstMatch = nullptr;
    for (int row = 0; row < m_sectionList->count(); ++row)
    {
        QListWidgetItem *item = m_sectionList->item(row);
        if (item == nullptr)
            continue;
        QString searchableText = item->text();
        QWidget *page = m_sectionStack->widget(item->data(Qt::UserRole).toInt());
        if (page != nullptr)
        {
            const QList<QLabel *> labels = page->findChildren<QLabel *>();
            for (const QLabel *label : labels)
                searchableText += QLatin1Char(' ') + label->text();
            const QList<QAbstractButton *> controls =
                page->findChildren<QAbstractButton *>();
            for (const QAbstractButton *control : controls)
                searchableText += QLatin1Char(' ') + control->text();
        }
        const bool matches = needle.isEmpty()
            || searchableText.contains(needle, Qt::CaseInsensitive);
        item->setHidden(!matches);
        if (matches && firstMatch == nullptr)
            firstMatch = item;
    }

    QListWidgetItem *current = m_sectionList->currentItem();
    if (firstMatch != nullptr && (current == nullptr || current->isHidden()))
        m_sectionList->setCurrentItem(firstMatch);
    m_sectionStack->setVisible(firstMatch != nullptr);
}

void PreferencesDialog::scheduleThemeRefresh()
{
    if (m_themeRefreshPending)
        return;

    m_themeRefreshPending = true;
    QTimer::singleShot(0, this, [this]() {
        reapplyTheme();
        m_themeRefreshPending = false;
    });
}

void PreferencesDialog::reapplyTheme()
{
    polishDialogPalette(this);
    setStyleSheet(preferencesStyleSheet());
}

void PreferencesDialog::populateSyntaxThemeCombos()
{
    const QString selectedDark = m_darkSyntaxThemeCombo->currentData().toString();
    const QString selectedLight = m_lightSyntaxThemeCombo->currentData().toString();
    const QSignalBlocker blockDark(m_darkSyntaxThemeCombo);
    const QSignalBlocker blockLight(m_lightSyntaxThemeCombo);
    m_darkSyntaxThemeCombo->clear();
    m_lightSyntaxThemeCombo->clear();
    for (const NotePeek::SyntaxTheme &theme : NotePeek::SyntaxThemeCatalog::themes())
    {
        QComboBox *combo = theme.dark ? m_darkSyntaxThemeCombo : m_lightSyntaxThemeCombo;
        combo->addItem(theme.name, theme.id);
    }
    const int darkIndex = m_darkSyntaxThemeCombo->findData(selectedDark);
    const int lightIndex = m_lightSyntaxThemeCombo->findData(selectedLight);
    if (darkIndex >= 0)
        m_darkSyntaxThemeCombo->setCurrentIndex(darkIndex);
    if (lightIndex >= 0)
        m_lightSyntaxThemeCombo->setCurrentIndex(lightIndex);
}

void PreferencesDialog::connectLiveUpdates()
{
    const auto applyChange = [this]() {
        commit();
    };

    connect(m_textFontCombo, &QFontComboBox::currentFontChanged,
            this, [applyChange](const QFont &) { applyChange(); });
    connect(m_textFontSize, &QComboBox::activated,
            this, [applyChange](int) { applyChange(); });
    connect(m_textFontSize->lineEdit(), &QLineEdit::editingFinished,
            this, applyChange);
    connect(m_codeFontCombo, &QComboBox::currentIndexChanged,
            this, [applyChange](int) { applyChange(); });
    connect(m_codeFontSize, &QComboBox::activated,
            this, [applyChange](int) { applyChange(); });
    connect(m_codeFontSize->lineEdit(), &QLineEdit::editingFinished,
            this, applyChange);
    connect(m_darkSyntaxThemeCombo, &QComboBox::currentIndexChanged,
            this, [applyChange](int) { applyChange(); });
    connect(m_lightSyntaxThemeCombo, &QComboBox::currentIndexChanged,
            this, [applyChange](int) { applyChange(); });

    const QList<QCheckBox *> checkBoxes = {
        m_showFileTree,
        m_showStatusBar,
        m_gitIntegrationEnabled,
        m_markdownSidePreview,
        m_codeFolding,
        m_bracketAssistance,
        m_smartIndentation,
        m_documentCompletion,
        m_semanticHighlighting,
    };
    for (QCheckBox *checkBox : checkBoxes)
    {
        connect(checkBox, &QCheckBox::toggled,
                this, [applyChange](bool) { applyChange(); });
    }

    const QList<QComboBox *> combos = {
        m_untitledModeCombo,
        m_overtypeMode,
        m_languageCombo,
    };
    for (QComboBox *combo : combos)
    {
        connect(combo, &QComboBox::currentIndexChanged,
                this, [applyChange](int) { applyChange(); });
    }
}

void PreferencesDialog::importSyntaxTheme()
{
    const QString sourcePath = QFileDialog::getOpenFileName(
        this, tr("Import syntax theme"), QString(), tr("JSON theme files (*.json)"));
    if (sourcePath.isEmpty())
        return;

    QFile source(sourcePath);
    if (!source.open(QFile::ReadOnly))
    {
        QMessageBox::warning(this, tr("Import theme"),
                             tr("The selected theme file could not be read."));
        return;
    }
    const QByteArray json = source.readAll();
    QString error;
    const NotePeek::SyntaxTheme theme =
        NotePeek::SyntaxThemeCatalog::fromJson(json, &error);
    if (!theme.isValid())
    {
        QMessageBox::warning(this, tr("Import theme"),
                             tr("The selected file is not a valid theme:\n%1").arg(error));
        return;
    }

    const QString directoryPath = NotePeek::SyntaxThemeCatalog::userThemeDirectory();
    if (!QDir().mkpath(directoryPath))
    {
        QMessageBox::warning(this, tr("Import theme"),
                             tr("The theme folder could not be created."));
        return;
    }
    QString fileName = theme.id;
    fileName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")),
                     QStringLiteral("_"));
    if (fileName.isEmpty())
        fileName = QFileInfo(sourcePath).completeBaseName();
    const QString destinationPath =
        QDir(directoryPath).filePath(fileName + QStringLiteral(".json"));
    QSaveFile destination(destinationPath);
    if (!destination.open(QFile::WriteOnly) || destination.write(json) != json.size()
        || !destination.commit())
    {
        destination.cancelWriting();
        QMessageBox::warning(this, tr("Import theme"),
                             tr("The theme file could not be saved."));
        return;
    }

    NotePeek::SyntaxThemeCatalog::reload();
    populateSyntaxThemeCombos();
    QComboBox *combo = theme.dark ? m_darkSyntaxThemeCombo : m_lightSyntaxThemeCombo;
    const int importedIndex = combo->findData(theme.id);
    if (importedIndex >= 0)
        combo->setCurrentIndex(importedIndex);
    QMessageBox::information(this, tr("Import theme"),
                             tr("Theme “%1” was imported.").arg(theme.name));
}

void PreferencesDialog::openSyntaxThemeFolder()
{
    const QString directoryPath = NotePeek::SyntaxThemeCatalog::userThemeDirectory();
    if (!QDir().mkpath(directoryPath)
        || !QDesktopServices::openUrl(QUrl::fromLocalFile(directoryPath)))
    {
        QMessageBox::warning(this, tr("Open theme folder"),
                             tr("The theme folder could not be opened."));
    }
}

void PreferencesDialog::loadFromPreferences()
{
    const AppPreferences &prefs = AppPreferences::instance();

    m_textFontCombo->setCurrentFont(QFont(prefs.textFontFamily()));
    setCurrentFontSize(m_textFontSize, prefs.textFontSize());
    setComboCurrentFont(m_codeFontCombo, QFont(prefs.codeFontFamily()));
    setCurrentFontSize(m_codeFontSize, prefs.codeFontSize());
    const int darkThemeIndex = m_darkSyntaxThemeCombo->findData(prefs.darkSyntaxThemeId());
    m_darkSyntaxThemeCombo->setCurrentIndex(darkThemeIndex >= 0 ? darkThemeIndex : 0);
    const int lightThemeIndex = m_lightSyntaxThemeCombo->findData(prefs.lightSyntaxThemeId());
    m_lightSyntaxThemeCombo->setCurrentIndex(lightThemeIndex >= 0 ? lightThemeIndex : 0);
    m_showFileTree->setChecked(prefs.showFileTreeOnStartup());
    m_showStatusBar->setChecked(prefs.showStatusBar());
    m_gitIntegrationEnabled->setChecked(prefs.gitIntegrationEnabled());
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
    prefs.setDarkSyntaxThemeId(m_darkSyntaxThemeCombo->currentData().toString());
    prefs.setLightSyntaxThemeId(m_lightSyntaxThemeCombo->currentData().toString());
    prefs.setShowFileTreeOnStartup(m_showFileTree->isChecked());
    prefs.setShowStatusBar(m_showStatusBar->isChecked());
    prefs.setGitIntegrationEnabled(m_gitIntegrationEnabled->isChecked());
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

    emit applied();
}
