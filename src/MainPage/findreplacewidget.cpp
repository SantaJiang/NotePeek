#include "findreplacewidget.h"
#include "ui_findreplacewidget.h"
#include "multieditor.h"
#include <QMessageBox>
#include <QKeyEvent>
#include <QLineEdit>
#include "../QScintilla/Qsci/qsciscintilla.h"

FindReplaceWidget::FindReplaceWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FindReplaceWidget)
    , m_pEditor(nullptr)
    , m_highlightIndicator(8) // 使用一个不常用的指示器编号..
{
    ui->setupUi(this);
    int fontId = QFontDatabase::addApplicationFont("://rescoure/fontawesome-webfont.ttf");
    QString fontName = QFontDatabase::applicationFontFamilies(fontId).at(0);
    QFont iconFont = QFont(fontName);
    ui->buttonClose->setFont(iconFont);
    ui->buttonClose->setText(QChar(0xf00d));

    // 连接按钮信号到槽函数..
    connect(ui->buttonFind, &QPushButton::clicked, this, &FindReplaceWidget::find);
    connect(ui->buttonReplace, &QPushButton::clicked, this, &FindReplaceWidget::replace);
    connect(ui->buttonReplaceAll, &QPushButton::clicked, this, &FindReplaceWidget::replaceAll);

    // 设置查找组合框的自动完成..
    ui->comboFind->setCompleter(nullptr);
    ui->comboReplace->setCompleter(nullptr);
    ui->checkBoxWrapAround->setChecked(true);
}

FindReplaceWidget::~FindReplaceWidget()
{
    delete ui;
}

void FindReplaceWidget::setMutiEditor(MultiEditor *pEditor)
{
    m_pEditor = pEditor;
}

void FindReplaceWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if(m_pEditor != nullptr)
    {
        QsciScintilla *editor = m_pEditor->currentEditor();
        if (editor)
        {
            // 初始化高亮指示器 - 使用自定义样式..
            editor->indicatorDefine(QsciScintilla::FullBoxIndicator, m_highlightIndicator);

            // 设置高亮颜色..
            QColor highlightColor("#F59E0B");

            // 使用低级别的Scintilla API设置指示器颜色..
            long color = (highlightColor.blue() << 16) | (highlightColor.green() << 8) | highlightColor.red();
            editor->SendScintilla(QsciScintillaBase::SCI_INDICSETFORE, m_highlightIndicator, color);

            // 设置指示器绘制在文本下方（作为背景）..
            editor->SendScintilla(QsciScintillaBase::SCI_INDICSETUNDER, m_highlightIndicator, true);

            // 设置指示器不绘制轮廓线..
            editor->SendScintilla(QsciScintillaBase::SCI_INDICSETOUTLINEALPHA, (unsigned long)m_highlightIndicator, (long)0);

            // 对话框显示时，获取当前选中的文本作为查找文本..
            QString selectedText = editor->selectedText();
            if (!selectedText.isEmpty())
            {
                ui->comboFind->setCurrentText(selectedText);
                m_initialFindText = selectedText;

                // 立即高亮所有匹配项..
                highlightAllMatches();
            }
            else
            {
                m_initialFindText.clear();
            }
        }
    }

    // 焦点设置到查找框..
    ui->comboFind->setFocus();
    ui->comboFind->lineEdit()->selectAll();
}

void FindReplaceWidget::hideEvent(QHideEvent *event)
{
    // 清除所有高亮..
    clearHighlights();
    QWidget::hideEvent(event);
}

void FindReplaceWidget::highlightAllMatches()
{
    if(m_pEditor == nullptr)
        return;

    QsciScintilla *editor = m_pEditor->currentEditor();
    if (!editor)
        return;

    QString filePath = editor->property("filePath").toString();
    QFileInfo fileInfo2(filePath);
    bool isLargeFile = (fileInfo2.size() > 10 * 1024 * 1024);

    if (isLargeFile)
    {
        return;
    }

    // 确保当前编辑器有正确的高亮指示器设置..
    editor->indicatorDefine(QsciScintilla::FullBoxIndicator, m_highlightIndicator);
    QColor highlightColor("#F59E0B");
    long color = (highlightColor.blue() << 16) | (highlightColor.green() << 8) | highlightColor.red();
    editor->SendScintilla(QsciScintillaBase::SCI_INDICSETFORE, m_highlightIndicator, color);
    editor->SendScintilla(QsciScintillaBase::SCI_INDICSETUNDER, m_highlightIndicator, true);
    editor->SendScintilla(QsciScintillaBase::SCI_INDICSETOUTLINEALPHA, (unsigned long)m_highlightIndicator, (long)0);

    QString searchText = ui->comboFind->currentText();
    if (searchText.isEmpty())
    {
        clearHighlights();
        return;
    }

    // 清除之前的高亮..
    clearHighlights();

    // 保存当前光标位置和选择..
    int originalLine, originalIndex;
    editor->getCursorPosition(&originalLine, &originalIndex);
    bool hadSelection = editor->hasSelectedText();
    int selLineFrom, selIndexFrom, selLineTo, selIndexTo;
    if (hadSelection)
    {
        editor->getSelection(&selLineFrom, &selIndexFrom, &selLineTo, &selIndexTo);
    }

    // 设置查找选项..
    bool re = false;
    bool cs = ui->checkBoxMatchCase->isChecked();
    bool wo = ui->checkBoxMatchWholeWord->isChecked();
    bool wrap = false; // 不高亮时不需要循环..
    bool forward = true;

    // 从文档开始查找所有匹配项..
    editor->setCursorPosition(0, 0);
    bool found = editor->findFirst(searchText, re, cs, wo, wrap, forward);

    while (found)
    {
        // 获取当前选择的范围..
        int lineFrom, indexFrom, lineTo, indexTo;
        editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);

        // 高亮这个匹配项..
        editor->fillIndicatorRange(lineFrom, indexFrom, lineTo, indexTo, m_highlightIndicator);

        // 查找下一个..
        found = editor->findNext();
    }

    // 恢复原始光标位置和选择..
    if (hadSelection)
    {
        editor->setSelection(selLineFrom, selIndexFrom, selLineTo, selIndexTo);
    }
    else
    {
        editor->setCursorPosition(originalLine, originalIndex);
    }

    // 如果是初始查找文本为空，现在有文本了，选择第一个匹配项..
    if (m_initialFindText.isEmpty() && !searchText.isEmpty())
    {
        selectFirstMatch();
    }
}

void FindReplaceWidget::clearHighlights()
{
    if(m_pEditor == nullptr)
        return;

    QsciScintilla *editor = m_pEditor->currentEditor();
    if (!editor)
        return;

    // 清除所有高亮指示器..
    editor->clearIndicatorRange(0, 0, editor->lines(), editor->lineLength(editor->lines() - 1), m_highlightIndicator);
}

void FindReplaceWidget::selectFirstMatch()
{
    if(m_pEditor == nullptr)
        return;

    QsciScintilla *editor = m_pEditor->currentEditor();
    if (!editor)
        return;

    QString searchText = ui->comboFind->currentText();
    if (searchText.isEmpty())
        return;

    // 设置查找选项..
    bool re = false;
    bool cs = ui->checkBoxMatchCase->isChecked();
    bool wo = ui->checkBoxMatchWholeWord->isChecked();
    bool wrap = ui->checkBoxWrapAround->isChecked();
    bool forward = !ui->checkBoxBackwardsDirection->isChecked();

    // 查找第一个匹配项并选中它..
    editor->findFirst(searchText, re, cs, wo, wrap, forward);
}

void FindReplaceWidget::find()
{
    if(m_pEditor == nullptr)
        return;

    // 通过主窗口获取当前编辑器..
    QsciScintilla *editor = m_pEditor->currentEditor();
    if (!editor)
    {
        QMessageBox::information(this, tr("Find"), tr("No active editor."));
        return;
    }

    QString selectedText = editor->selectedText();
    if (!selectedText.isEmpty())
    {
        ui->comboFind->setCurrentText(selectedText);
        m_initialFindText = selectedText;

        // 立即高亮所有匹配项..
        highlightAllMatches();
    }

    ui->comboFind->setFocus();
    ui->comboFind->lineEdit()->selectAll();
    highlightAllMatches();
    // 添加到查找历史..
    addToHistory(ui->comboFind, ui->comboFind->currentText());

    // 设置查找选项..
    bool re = false; // 不使用正则表达式..
    bool cs = ui->checkBoxMatchCase->isChecked();
    bool wo = ui->checkBoxMatchWholeWord->isChecked();
    bool wrap = ui->checkBoxWrapAround->isChecked();
    bool forward = !ui->checkBoxBackwardsDirection->isChecked();

    // 执行查找..
    editor->findFirst(ui->comboFind->currentText(), re, cs, wo, wrap, forward);
}

void FindReplaceWidget::findNext()
{
    if(m_pEditor == nullptr)
        return;

    // 通过主窗口获取当前编辑器..
    QsciScintilla *editor = m_pEditor->currentEditor();
    if (!editor)
    {
        QMessageBox::information(this, tr("Find"), tr("No active editor."));
        return;
    }

    if (ui->comboFind->currentText().isEmpty())
    {
        find();
        return;
    }

    // 使用相同的选项继续查找下一个..
    bool found = editor->findNext();

    if (!found && ui->checkBoxWrapAround->isChecked())
    {
        // 如果启用了循环搜索，尝试从文档开头/结尾重新查找..
        bool forward = !ui->checkBoxBackwardsDirection->isChecked();
        int line = forward ? 0 : editor->lines() - 1;
        int index = forward ? 0 : editor->lineLength(line);

        QString searchText = ui->comboFind->currentText();
        bool re = false;
        bool cs = ui->checkBoxMatchCase->isChecked();
        bool wo = ui->checkBoxMatchWholeWord->isChecked();
        bool wrap = false; // 禁用循环以避免无限递归..

        found = editor->findFirst(searchText, re, cs, wo, wrap, forward, line, index);
    }
}

void FindReplaceWidget::refreshHighlights()
{
    if (isVisible())
    {
        clearHighlights();
        highlightAllMatches();
    }
}

void FindReplaceWidget::replace()
{
    if(m_pEditor == nullptr)
        return;

    // 通过主窗口获取当前编辑器..
    QsciScintilla *editor = m_pEditor->currentEditor();
    if (!editor)
    {
        QMessageBox::information(this, tr("Replace"), tr("No active editor."));
        return;
    }

    QString replaceText = ui->comboReplace->currentText();

    // 添加到替换历史..
    addToHistory(ui->comboReplace, replaceText);

    // 如果有选中文本，替换它..
    if (editor->hasSelectedText())
    {
        editor->replaceSelectedText(replaceText);
    }

    // 查找下一个匹配项..
    findNext();
}

void FindReplaceWidget::replaceAll()
{
    if(m_pEditor == nullptr)
        return;

    // 通过主窗口获取当前编辑器..
    QsciScintilla *editor = m_pEditor->currentEditor();
    if (!editor)
    {
        QMessageBox::information(this, tr("Replace All"), tr("No active editor."));
        return;
    }

    QString searchText = ui->comboFind->currentText();
    QString replaceText = ui->comboReplace->currentText();

    if (searchText.isEmpty())
    {
        QMessageBox::information(this, tr("Replace All"), tr("The search field is empty."));
        return;
    }

    // 添加到历史..
    addToHistory(ui->comboFind, searchText);
    addToHistory(ui->comboReplace, replaceText);

    // 保存当前光标位置..
    int currentLine, currentIndex;
    editor->getCursorPosition(&currentLine, &currentIndex);

    // 保存当前查找选项..
    bool re = false;
    bool cs = ui->checkBoxMatchCase->isChecked();
    bool wo = ui->checkBoxMatchWholeWord->isChecked();

    // 移动到文档开始..
    editor->setCursorPosition(0, 0);

    // 执行查找并替换所有..
    int replaceCount = 0;
    bool found = editor->findFirst(searchText, re, cs, wo, false, true);

    while (found)
    {
        editor->replaceSelectedText(replaceText);
        replaceCount++;
        found = editor->findNext();
    }

    // 恢复光标位置..
    editor->setCursorPosition(currentLine, currentIndex);

    // 显示替换结果..
    QMessageBox::information(this, tr("Replace All"),
                             tr("Replaced %1 occurrence(s).").arg(replaceCount));
}

void FindReplaceWidget::addToHistory(QComboBox *combo, const QString &text)
{
    if (text.isEmpty())
    {
        return;
    }

    int index = combo->findText(text);
    if (index != -1)
    {
        combo->removeItem(index);
    }

    combo->insertItem(0, text);
    combo->setCurrentIndex(0);

    // 限制历史记录数量..
    while (combo->count() > 10)
    {
        combo->removeItem(combo->count() - 1);
    }
}

void FindReplaceWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        close();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        // 判断焦点在哪个控件上..
        if (ui->comboFind->hasFocus())
        {
            find();
            return;
        }
        else if (ui->comboReplace->hasFocus())
        {
            replace();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}
