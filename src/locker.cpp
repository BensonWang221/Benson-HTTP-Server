#include <exception>
#include "locker.h"

Sem::Sem()
{
    if (sem_init(&m_sem, 0, 0) != 0)
        throw std::exception();
}

Sem::~Sem()
{
    sem_destroy(&m_sem);
}

bool Sem::Wait()
{
    return sem_wait(&m_sem) == 0;
}

bool Sem::Post()
{
    return sem_post(&m_sem) == 0;
}