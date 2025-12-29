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
class MultiEditor;

class FindReplaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FindReplaceWidget(QWidget *parent = nullptr);
    ~FindReplaceWidget();

    void setMultiEditor(MultiEditor *multiEditor);
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

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onCloseClicked();
    void onFindTextChanged(const QString &text);
    void onReplaceTextChanged(const QString &text);
    void onCaseSensitiveToggled(bool checked);
    void onWholeWordToggled(bool checked);
    void onWrapAroundToggled(bool checked);
    void onBackwardsToggled(bool checked);

private:
    Ui::FindReplaceWidget *ui;
    FindReplaceManager *m_manager;
    MultiEditor *m_multiEditor;
};

#endif // FINDREPLACEWIDGET_H
