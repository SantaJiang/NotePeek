#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFontComboBox;

// The application settings window (File ▸ Preferences).  Editing is
// non-destructive: changes are only committed to AppPreferences when the user
// presses Apply or OK, at which point applied() is emitted so the main window
// can refresh open editors live.  Cancel discards everything.
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

    QFontComboBox *m_textFontCombo;
    QComboBox *m_textFontSize;
    QFontComboBox *m_codeFontCombo;
    QComboBox *m_codeFontSize;
    QCheckBox *m_showFileTree;
    QCheckBox *m_markdownSidePreview;
};

#endif // PREFERENCESDIALOG_H
