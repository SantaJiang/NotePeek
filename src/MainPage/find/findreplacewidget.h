#ifndef FINDREPLACEWIDGET_H
#define FINDREPLACEWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QKeyEvent>

namespace Ui
{
class FindReplaceWidget;
}

class FindReplaceManager;

class FindReplaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FindReplaceWidget(QWidget *parent = nullptr);
    ~FindReplaceWidget();

    void setFindReplaceManager(FindReplaceManager *manager);
    void showWithInitialText(const QString &initialText = QString());

    void refreshHighlights();
    void findNext();
    void setFocus();

public slots:
    void find();
    void onFindClicked();
    void onReplaceClicked();
    void onReplaceAllClicked();

signals:
    void visibilityChanged(bool visible);
    void advancedSearchRequested();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onFindTextChanged(const QString &text);
    void onReplaceTextChanged(const QString &text);
    void onCaseSensitiveToggled(bool checked);
    void onWholeWordToggled(bool checked);
    void onWrapAroundToggled(bool checked);
    void onBackwardsToggled(bool checked);

private:
    void applyChromeTheme();

    Ui::FindReplaceWidget *ui;
    FindReplaceManager *m_manager;
};

#endif // FINDREPLACEWIDGET_H
