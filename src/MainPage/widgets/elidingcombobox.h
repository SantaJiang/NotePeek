#ifndef ELIDINGCOMBOBOX_H
#define ELIDINGCOMBOBOX_H

#include <QComboBox>
#include <QStyleOptionComboBox>
#include <QStylePainter>

class ElidingComboBox final : public QComboBox
{
public:
    using QComboBox::QComboBox;

    QString elidedCurrentText() const
    {
        QStyleOptionComboBox option;
        initStyleOption(&option);
        const QRect textRect = comboTextRect(option);
        return fontMetrics().elidedText(currentText(), Qt::ElideRight, textRect.width());
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        if (isEditable())
        {
            QComboBox::paintEvent(event);
            return;
        }

        Q_UNUSED(event)

        QStylePainter painter(this);
        QStyleOptionComboBox option;
        initStyleOption(&option);

        option.currentText.clear();
        painter.drawComplexControl(QStyle::CC_ComboBox, option);

        // CE_ComboBoxLabel applies the style's content margins again. Drawing
        // directly in SC_ComboBoxEditField avoids subtracting the combo-box
        // padding and drop-down area twice.
        painter.drawItemText(comboTextRect(option),
                             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                             option.palette,
                             option.state.testFlag(QStyle::State_Enabled),
                             elidedCurrentText(),
                             QPalette::ButtonText);
    }

private:
    QRect comboTextRect(const QStyleOptionComboBox &option) const
    {
        return style()->subControlRect(QStyle::CC_ComboBox,
                                       &option,
                                       QStyle::SC_ComboBoxEditField,
                                       this);
    }
};

#endif // ELIDINGCOMBOBOX_H
