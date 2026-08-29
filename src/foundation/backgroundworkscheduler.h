#ifndef NOTEPEEK_FOUNDATION_BACKGROUNDWORKSCHEDULER_H
#define NOTEPEEK_FOUNDATION_BACKGROUNDWORKSCHEDULER_H

class QThreadPool;

// Process-wide bounded lanes for asynchronous work. Centralizing the pools
// prevents each window/service from multiplying its own worker count and
// oversubscribing low-core machines.
class BackgroundWorkScheduler
{
public:
    static QThreadPool *interactivePool();
    static QThreadPool *ioPool();
    static QThreadPool *backgroundPool();
};

#endif // NOTEPEEK_FOUNDATION_BACKGROUNDWORKSCHEDULER_H
