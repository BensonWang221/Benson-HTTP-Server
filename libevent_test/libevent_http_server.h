#include <event2/http.h>
#include <event2/event.h>
#include "ThreadPool.h"

class LibeventServer
{
public:

    LibeventServer();

    void AddTask(ThreadPool::Task t);

    void Run();

private:
    struct event_base* m_base = nullptr;
    struct evhttp* m_evh = nullptr;
    ThreadPool* m_threadPool = nullptr;
};