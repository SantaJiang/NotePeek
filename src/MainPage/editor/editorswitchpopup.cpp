#include "editorswitchpopup.h"
#include <QDebug>

EditorSwitchPopup::EditorSwitchPopup(QWidget *parent)
    : QDialog(parent), m_ctrlPressed(false)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_listWidget = new QListWidget(this);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_listWidget);

    // 安装事件过滤器来捕获列表项的点击事件..
    m_listWidget->installEventFilter(this);

    // 连接项选择信号..
    connect(m_listWidget, &QListWidget::itemClicked, [this](QListWidgetItem *item)
    {
        emit itemSelected(m_listWidget->row(item));
        close();
    });

    // 设置样式，使其看起来像组合框的下拉列表..
    setStyleSheet("QListWidget { border: 1px solid #C0C0C0; background-color: white; }"
                  "QListWidget::item { padding: 2px; }"
                  "QListWidget::item:selected { background-color: #316AC5; color: white; }");
}

void EditorSwitchPopup::setItems(const QStringList &items, const QStringList &toolTips)
{
    m_listWidget->clear();
    m_listWidget->addItems(items);

    for (int i = 0; i < items.size() && i < toolTips.size(); ++i)
    {
        if (!toolTips[i].isEmpty())
        {
            m_listWidget->item(i)->setToolTip(toolTips[i]);
        }
    }
}

void EditorSwitchPopup::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_listWidget->count())
    {
        m_listWidget->setCurrentRow(index);
        m_listWidget->scrollToItem(m_listWidget->item(index));
    }
}

int EditorSwitchPopup::currentIndex() const
{
    return m_listWidget->currentRow();
}

QString EditorSwitchPopup::currentText() const
{
    QListWidgetItem *item = m_listWidget->currentItem();
    return item ? item->text() : QString();
}

void EditorSwitchPopup::showPopup()
{
    setFocus();
    m_listWidget->setFocus();
    m_ctrlPressed = true; // 初始状态为Ctrl按下..

    if (m_listWidget->currentRow() < m_listWidget->count() - 1)
    {
        m_listWidget->setCurrentRow(m_listWidget->currentRow() + 1);
    }
    else
    {
        m_listWidget->setCurrentRow(0);
    }

    exec();
}

void EditorSwitchPopup::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control)
    {
        m_ctrlPressed = true;
        event->accept();
        return;
    }

    if (m_ctrlPressed)
    {
        switch (event->key())
        {
        case Qt::Key_Tab:
            // 向下选择..
            if (m_listWidget->currentRow() < m_listWidget->count() - 1)
            {
                m_listWidget->setCurrentRow(m_listWidget->currentRow() + 1);
            }
            else
            {
                m_listWidget->setCurrentRow(0);
            }
            event->accept();
            return;

        case Qt::Key_Backtab:
            // 向上选择（Shift+Tab）..
            if (m_listWidget->currentRow() > 0)
            {
                m_listWidget->setCurrentRow(m_listWidget->currentRow() - 1);
            }
            else
            {
                m_listWidget->setCurrentRow(m_listWidget->count() - 1);
            }
            event->accept();
            return;

        case Qt::Key_Enter:
        case Qt::Key_Return:
            // 确认选择..
            emit itemSelected(m_listWidget->currentRow());
            close();
            event->accept();
            return;

        case Qt::Key_Escape:
            // 取消选择..
            close();
            event->accept();
            return;
        }
    }

    QDialog::keyPressEvent(event);
}

void EditorSwitchPopup::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Alt)
    {
        m_ctrlPressed = false;
        // Ctrl键释放时，如果当前有选中项，则触发选择..
        if (m_listWidget->currentRow() >= 0)
        {
            emit itemSelected(m_listWidget->currentRow());
        }
        close();
        event->accept();
        return;
    }

    QDialog::keyReleaseEvent(event);
}

void EditorSwitchPopup::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event)
    // 失去焦点时关闭弹窗..
    emit popupClosed();
    close();
}

bool EditorSwitchPopup::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_listWidget)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            keyPressEvent(keyEvent);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
