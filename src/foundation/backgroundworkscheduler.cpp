#include "foundation/backgroundworkscheduler.h"

#include <QThreadPool>

namespace {

class BoundedThreadPool final : public QThreadPool
{
public:
    BoundedThreadPool(int maximumThreads, int expiryTimeoutMs)
    {
        setMaxThreadCount(maximumThreads);
        setExpiryTimeout(expiryTimeoutMs);
    }
};

} // namespace

QThreadPool *BackgroundWorkScheduler::interactivePool()
{
    static BoundedThreadPool pool(2, 30000);
    return &pool;
}

QThreadPool *BackgroundWorkScheduler::ioPool()
{
    static BoundedThreadPool pool(2, 30000);
    return &pool;
}

QThreadPool *BackgroundWorkScheduler::backgroundPool()
{
    static BoundedThreadPool pool(1, 10000);
    return &pool;
}
