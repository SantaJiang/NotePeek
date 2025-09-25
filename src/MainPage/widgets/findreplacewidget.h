#ifndef FINDREPLACEWIDGET_H
#define FINDREPLACEWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QKeyEvent>
#include <QTimer>

class MultiEditor;

namespace Ui
{
class FindReplaceWidget;
}

class FindReplaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FindReplaceWidget(QWidget *parent = nullptr);
    ~FindReplaceWidget();
    void setMutiEditor(MultiEditor *pEditor);
    void findNext();
    void refreshHighlights();

protected:
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void hideEvent(QHideEvent *event) override;


public slots:
    void find();

private slots:
    void replace();
    void replaceAll();
    void highlightAllMatches();
    void addToHistory(QComboBox *combo, const QString &text);

private:
    Ui::FindReplaceWidget *ui;
    MultiEditor *m_pEditor;

    int m_highlightIndicator; // 用于高亮匹配项的指示器..
    QString m_initialFindText; // 记录初始查找文本..

    void clearHighlights();
    void selectFirstMatch();
};

#endif // FINDREPLACEWIDGET_H
