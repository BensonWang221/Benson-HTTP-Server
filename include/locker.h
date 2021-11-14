#ifndef LOCKER_INCLUDED
#define LOCKER_INCLUDED

#include <semaphore.h>
#include <pthread.h>
#include <exception>

class Sem
{
public:
    Sem();

    ~Sem();

    bool Wait();

    bool Post();

private:
    sem_t m_sem;
};

template <class T>
class MyRWLock
{
public:
    MyRWLock() 
    {
        Init();
    }

    MyRWLock(const T& element) : m_element(element)
    {
        Init();
    }
    ~MyRWLock()
    {
        m_element.~T();
        pthread_rwlock_destroy(&m_rwlock);
    }

    const T& Read()
    {
        pthread_rwlock_rdlock(&m_rwlock);
        const T& ret = m_element;
        pthread_rwlock_unlock(&m_rwlock);
        return ret;
    }

    void Write(const T& element)
    {
        pthread_rwlock_wrlock(&m_rwlock);
        m_element = element;
        pthread_rwlock_unlock(&m_rwlock);
    }
    
private:
    void Init()
    {
        if (pthread_rwlock_init(&m_rwlock, nullptr) != 0)
            throw std::exception();
    }

    T m_element;
    pthread_rwlock_t m_rwlock;
};
#endif