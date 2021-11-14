#include "ThreadPool.h"

using namespace std;

namespace
{
    const int tasksEachTime = 3;
}

ThreadPool::ThreadPool(int numThreads) : m_running(false)
{
    m_threadGroup.reserve(numThreads);
    Start(numThreads);
}

ThreadPool::~ThreadPool()
{
    Stop();
}

void ThreadPool::Stop()
{
    call_once(m_onceFlag, [this]() { this->StopThreadGroup();});
}

void ThreadPool::AddTask(Task&&task)
{
    m_queue.Put(forward<Task>(task));
}

void ThreadPool::AddTask(const Task& task)
{
    m_queue.Put(task);
}

void ThreadPool::Start(int numThreads)
{
    m_running.Write(true);
    for (int i = 0; i < numThreads; ++i)
        m_threadGroup.push_back(make_shared<thread>(&ThreadPool::RunInThread, this));
}

void ThreadPool::RunInThread()
{
    while(m_running.Read())
    {
        list<Task> todoTasks;
        m_queue.Take(todoTasks, tasksEachTime);

        for (auto& task: todoTasks)
        {
            // 中途stop的话不再继续
            if (!m_running.Read())
                return;
            task();
        }
        /*Task task;
        m_queue.Take(task);
        if (!m_running.Read())
            return;
        task();*/
    }
}

void ThreadPool::StopThreadGroup()
{
    // 先让任务队列stop
    m_queue.Stop();
    m_running.Write(false);

    for (auto thread: m_threadGroup)
        thread->join();
    
    m_threadGroup.clear();
}

#ifdef TEST
size_t ThreadPool::GetTasksInQueue()
{
    return m_queue.Size();
}
#endif