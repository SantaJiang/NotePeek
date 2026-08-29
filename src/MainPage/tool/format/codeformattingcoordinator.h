#ifndef CODEFORMATTINGCOORDINATOR_H
#define CODEFORMATTINGCOORDINATOR_H

#include <QObject>

#include <functional>

class CustomEditor;
class QWidget;

// Coordinates one whole-document formatting request without owning editor or
// window state. Pure formatting remains in CodeFormatter; this class owns the
// async/revision/undo/UI boundary.
class CodeFormattingCoordinator final : public QObject
{
    Q_OBJECT

public:
    using FocusEligibility = std::function<bool(CustomEditor *)>;

    explicit CodeFormattingCoordinator(QWidget *dialogParent,
                                       FocusEligibility focusEligibility,
                                       QObject *parent = nullptr);

    void format(CustomEditor *editor);

signals:
    void formattingStateChanged();

private:
    QWidget *m_dialogParent;
    FocusEligibility m_focusEligibility;
};

#endif // CODEFORMATTINGCOORDINATOR_H
