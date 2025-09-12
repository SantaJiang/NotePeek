#ifndef EDITORSWITCHPOPUP_H
#define EDITORSWITCHPOPUP_H

#include <QDialog>
#include <QListWidget>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QApplication>

class EditorSwitchPopup : public QDialog
{
    Q_OBJECT

public:
    explicit EditorSwitchPopup(QWidget *parent = nullptr);
    void setItems(const QStringList &items, const QStringList &toolTips = QStringList());
    void setCurrentIndex(int index);
    int currentIndex() const;
    QString currentText() const;
    void showPopup();

signals:
    void itemSelected(int index);
    void popupClosed();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QListWidget *m_listWidget;
    bool m_ctrlPressed;
};

#endif // EDITORSWITCHPOPUP_H
