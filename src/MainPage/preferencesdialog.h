#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QEvent;
class QFontComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;

// The application settings window (File ▸ Preferences). Changes are persisted
// immediately and applied to the main window while the dialog remains open.
//
// The dialog uses a two-pane layout: a left sidebar lists the available
// setting sections and the right pane shows the corresponding controls.  Only
// one section is visible at a time, switched by selecting a sidebar entry.
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

signals:
    // Emitted after each user change has been written and saved, so the rest
    // of the application can re-apply it without a restart.
    void applied();

private:
    void loadFromPreferences();
    void commit();
    void connectLiveUpdates();
    void populateSyntaxThemeCombos();
    void importSyntaxTheme();
    void openSyntaxThemeFolder();
    void filterSections(const QString &query);
    void scheduleThemeRefresh();
    void reapplyTheme();

    QFontComboBox *m_textFontCombo;
    QComboBox *m_textFontSize;
    QComboBox *m_codeFontCombo;
    QComboBox *m_codeFontSize;
    QComboBox *m_darkSyntaxThemeCombo;
    QComboBox *m_lightSyntaxThemeCombo;
    QPushButton *m_importSyntaxThemeButton;
    QPushButton *m_openSyntaxThemeFolderButton;
    QWidget *m_darkSyntaxThemeRow = nullptr;
    QWidget *m_lightSyntaxThemeRow = nullptr;
    QCheckBox *m_showFileTree;
    QCheckBox *m_showStatusBar;
    QCheckBox *m_revealCurrentFileInTree;
    QCheckBox *m_gitIntegrationEnabled;
    QComboBox *m_markdownPreviewOpenMode;
    QComboBox *m_untitledModeCombo;
    QComboBox *m_overtypeMode;
    QCheckBox *m_codeFolding;
    QCheckBox *m_bracketAssistance;
    QCheckBox *m_smartIndentation;
    QCheckBox *m_documentCompletion;
    QCheckBox *m_semanticHighlighting;
    QComboBox *m_languageCombo;
    QComboBox *m_colorThemeCombo;
    QLineEdit *m_searchEdit;
    QListWidget *m_sectionList;
    QStackedWidget *m_sectionStack;
    bool m_themeRefreshPending = false;
};

#endif // PREFERENCESDIALOG_H
