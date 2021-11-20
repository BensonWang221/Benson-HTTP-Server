#ifndef TIMER_INCLUDED
#define TIMER_INCLUDED

#include <chrono>

class MyTimer
{
public:
    MyTimer();

    void Update();

    double GetElapsedTimeInSecond() const;

    double GetElapsedTimeInMilliSec() const;

    double GetElapsedTimeInMicroSec() const;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_begin;
};

#endif