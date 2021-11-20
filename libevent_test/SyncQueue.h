/*
    分发/获取任务的任务队列
*/

#include <list>
#include <thread>
#include <iostream>
#include <condition_variable>

template <class T>
class SyncQueue
{
public:
    explicit SyncQueue() = default;

    SyncQueue(int maxSize) : m_maxSize(maxSize), m_needStop(false) {}

    void Put(const T& task);

    void Put(T &&task);

    void Take(T &task);

    // 一次取出n个任务，提高并发性
    void Take(std::list<T> &tasks, int n = 2);

    void Stop();

    bool Empty() const;

    bool Full() const;

    size_t Size() const;

    int Count() const;

private:
    bool NotFull() const;

    bool NotEmpty() const;

    template <class F>
    void Add(F &&task)
    {
        std::unique_lock<std::mutex> locker(m_mtx);
        m_notFull.wait(locker, [this](){ return this->m_needStop || this->NotFull();});
        if (m_needStop)
            return;
        m_tasks.push_front(std::forward<F>(task));
        m_notEmpty.notify_one();
    }

private:
    std::list<T> m_tasks;
    mutable std::mutex m_mtx;
    std::condition_variable m_notEmpty;
    std::condition_variable m_notFull;
    int m_maxSize = 10000;
    bool m_needStop = false;
};

template <class T>
void SyncQueue<T>::Put(const T& task)
{
    Add(task);
}

template <class T>
void SyncQueue<T>::Put(T &&task)
{
    Add(std::forward<T>(task));
}

template <class T>
void SyncQueue<T>::Take(T &task)
{
    std::unique_lock<std::mutex> locker(m_mtx);
    // 当队列非空或者stop时，停止阻塞等待
    m_notEmpty.wait(locker, [this]()
                    { return this->m_needStop || this->NotEmpty(); });
    
    // 当stop时会直接return，因此线程池如果有线程在阻塞等待也会返回
    if (m_needStop)
        return;
    task = m_tasks.back();
    m_tasks.pop_back();
    m_notFull.notify_one();
}

template <class T>
void SyncQueue<T>::Take(std::list<T> &tasks, int n)
{
    std::unique_lock<std::mutex> locker(m_mtx);
    m_notEmpty.wait(locker, [this]()
                    { return this->m_needStop || this->NotEmpty(); });
    if (m_needStop)
        return;

    auto startIter = n >= m_tasks.size() ? m_tasks.begin() : std::prev(m_tasks.end(), n + 1);
    tasks.assign(startIter, m_tasks.end());
    m_tasks.erase(startIter, m_tasks.end());
    m_notFull.notify_one();
}

template <class T>
void SyncQueue<T>::Stop()
{
    {
        std::lock_guard<std::mutex> locker(m_mtx);
        m_needStop = true;
    }
    // nofity放到lock的外面，更好的并发
    m_notFull.notify_all();
    m_notEmpty.notify_all();
}

template <class T>
bool SyncQueue<T>::Empty() const
{
    std::lock_guard<std::mutex> locker(m_mtx);
    return m_tasks.empty();
}

template <class T>
bool SyncQueue<T>::Full() const
{
    std::lock_guard<std::mutex> locker(m_mtx);
    return m_tasks.size() == m_maxSize;
}

template <class T>
size_t SyncQueue<T>::Size() const
{
    std::lock_guard<std::mutex> locker(m_mtx);
    return m_tasks.size();
}

template <class T>
int SyncQueue<T>::Count() const
{
    return m_tasks.size();
}

template <class T>
bool SyncQueue<T>::NotFull() const
{
    // 注意此处不能加锁，调用的地方在wait处，已经对m_mtx加锁
    bool full = m_tasks.size() >= m_maxSize;
    if (full)
        std::cout << "SyncQueue Full, thread id = " << std::this_thread::get_id() << std::endl;
    
    return !full;
}

template <class T>
bool SyncQueue<T>::NotEmpty() const
{
    bool empty = m_tasks.empty();
    /*if (empty)
    {
        std::cout << "Syncqueue empty, need wait, thread id = "
                  << std::this_thread::get_id() << std::endl;
    }*/
    return !empty;
}
