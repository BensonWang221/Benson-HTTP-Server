#ifndef THREAD_POOL_INCLUDED
#define THREAD_POOL_INCLUDED
#include <atomic>
#include <vector>
#include <functional>
#include <mutex>
#include <memory>
#include "SyncQueue.h"
#include "locker.h"

class ThreadPool
{
public:
    using Task = std::function<void()>;

    explicit ThreadPool(int numThreads = std::thread::hardware_concurrency());

    ~ThreadPool();

    void Stop();

    void AddTask(Task&& task);

    void AddTask(const Task& task);

    #ifdef TEST
    size_t GetTasksInQueue();
    #endif

private:
    void Start(int numThreads);

    void StopThreadGroup();

    void RunInThread();

private:
    std::vector<std::shared_ptr<std::thread>> m_threadGroup;
    SyncQueue<Task> m_queue;
    MyRWLock<bool> m_running;
    std::once_flag m_onceFlag;
};
#endif
