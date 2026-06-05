#ifndef FILETREEVIEW_H
#define FILETREEVIEW_H

#include <QTreeView>

class FileTreeView : public QTreeView
{
    Q_OBJECT

public:
    explicit FileTreeView(QWidget *parent = nullptr);

signals:
    void openInNewWindowRequested(const QString &filePath);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QString filePathForIndex(const QModelIndex &index) const;
    bool isFileIndex(const QModelIndex &index) const;

    QPoint m_dragStartPos;
};

#endif // FILETREEVIEW_H
