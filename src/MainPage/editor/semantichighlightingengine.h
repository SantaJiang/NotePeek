#ifndef SEMANTICHIGHLIGHTINGENGINE_H
#define SEMANTICHIGHLIGHTINGENGINE_H

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <NotePeek/lexerfactory.h>
#include <NotePeek/semantictokens.h>

#include <atomic>
#include <memory>

class CustomEditor;

// Viewport-bounded semantic analysis service. Keeping it separate from
// EditorManager isolates worker lifecycle, cancellation and cache policy from
// editor construction and appearance configuration.
class SemanticHighlightingEngine final : public QObject
{
public:
    explicit SemanticHighlightingEngine(QObject *parent = nullptr);
    ~SemanticHighlightingEngine() override;

    void attach(CustomEditor *editor);
    void setPath(CustomEditor *editor, const QString &path);
    void refreshTheme(CustomEditor *editor);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct State : QObject
    {
        explicit State(QObject *parent) : QObject(parent), timer(this) {}
        QPointer<CustomEditor> editor;
        QTimer timer;
        QString path;
        LexerFactory::Language language = LexerFactory::Unknown;
        quint64 requestId = 0;
        bool inFlight = false;
        bool rerun = false;
        std::shared_ptr<std::atomic_bool> cancellation;
    };

    static constexpr int kEditDebounceMs = 120;
    static constexpr int kScrollDebounceMs = 45;
    static constexpr int kLookBehindLines = 40;
    static constexpr int kLookAheadLines = 80;
    static constexpr int kMaxCachedTokens = 12000;
    static constexpr qint64 kWorkerBudgetNs = 2 * 1000 * 1000;

    void schedule(State *state, int delayMs);
    void cancel(State *state);
    void start(State *state);
    static NotePeek::SemanticEntryState entryStateForLine(CustomEditor *editor, int line);
    static QString viewportText(CustomEditor *editor, int firstLine, int lastLine);

    std::shared_ptr<const NotePeek::SemanticTokenProvider> m_provider;
    QHash<CustomEditor *, State *> m_states;
    QHash<QObject *, State *> m_eventTargets;
};

#endif // SEMANTICHIGHLIGHTINGENGINE_H
