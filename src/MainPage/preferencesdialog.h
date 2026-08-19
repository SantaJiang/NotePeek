#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;

// The application settings window (File ▸ Preferences).  Editing is
// non-destructive: changes are only committed to AppPreferences when the user
// presses Apply or OK, at which point applied() is emitted so the main window
// can refresh open editors live.  Cancel discards everything.
//
// The dialog uses a two-pane layout: a left sidebar lists the available
// setting sections and the right pane shows the corresponding controls.  Only
// one section is visible at a time, switched by selecting a sidebar entry.
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);

signals:
    // Emitted after preferences have been written and saved (Apply or OK), so
    // the rest of the application can re-apply them without a restart.
    void applied();

private:
    void loadFromPreferences();
    void commit();
    void populateSyntaxThemeCombos();
    void importSyntaxTheme();
    void openSyntaxThemeFolder();

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
    QCheckBox *m_markdownSidePreview;
    QComboBox *m_untitledModeCombo;
    QComboBox *m_overtypeMode;
    QCheckBox *m_codeFolding;
    QCheckBox *m_bracketAssistance;
    QCheckBox *m_smartIndentation;
    QCheckBox *m_documentCompletion;
    QCheckBox *m_semanticHighlighting;
    QComboBox *m_languageCombo;
    QListWidget *m_sectionList;
    QStackedWidget *m_sectionStack;
};

#endif // PREFERENCESDIALOG_H
