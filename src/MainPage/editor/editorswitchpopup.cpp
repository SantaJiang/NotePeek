#include "editor/editorswitchpopup.h"
#include "themecolors.h"

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

    // Intercept the list widget's events for keyboard handling
    m_listWidget->installEventFilter(this);

    connect(m_listWidget, &QListWidget::itemClicked, [this](QListWidgetItem *item)
    {
        emit itemSelected(m_listWidget->row(item));
        close();
    });

    // Apply theme-aware stylesheet
    applyThemeStyleSheet();
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
    // Re-apply stylesheet in case the theme has changed since last show
    applyThemeStyleSheet();

    setFocus();
    m_listWidget->setFocus();
    m_ctrlPressed = true; // The popup is opened while Ctrl is held

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
            // Select next item, wrapping to the top
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
            // Shift+Tab: select previous item, wrapping to the bottom
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
            emit itemSelected(m_listWidget->currentRow());
            close();
            event->accept();
            return;

        case Qt::Key_Escape:
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
        // Releasing Ctrl commits the current selection
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

void EditorSwitchPopup::applyThemeStyleSheet()
{
    using namespace ThemeColors::SwitchPopup;
    setStyleSheet(
        QStringLiteral("QListWidget { border: 1px solid %1; background-color: %2; color: %3; }"
                       "QListWidget::item { padding: 2px; }"
                       "QListWidget::item:selected { background-color: %4; color: %5; }")
            .arg(border().name(QColor::HexRgb),
                 background().name(QColor::HexRgb),
                 text().name(QColor::HexRgb),
                 selectedItemBg().name(QColor::HexRgb),
                 selectedItemFg().name(QColor::HexRgb)));
}
