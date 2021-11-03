#include <atomic>
#include <vector>
#include <functional>
#include <mutex>
#include "SyncQueue.h"

class ThreadPool
{
public:
    using Task = std::function<void()>;

    explicit ThreadPool(int numThreads = std::thread::hardware_concurrency());

    ~ThreadPool();

    void Stop();

    void AddTask(Task&& task);

    void AddTask(const Task& task);


private:
    void Start(int numThreads);

    void StopThreadGroup();

    void RunInThread();

private:
    std::vector<std::thread> m_threadGroup;
    SyncQueue<Task> m_queue;
    std::atomic_bool m_running;
    std::once_flag m_onceFlag;
};