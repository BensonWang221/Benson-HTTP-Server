#ifndef LOCKER_INCLUDED
#define LOCKER_INCLUDED

#include <semaphore.h>

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
#endif