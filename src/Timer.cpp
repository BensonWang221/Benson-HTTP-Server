#include "Timer.h"

using namespace std::chrono;

MyTimer::MyTimer() : m_begin(high_resolution_clock::now())
{
}

void MyTimer::Update()
{
    m_begin = high_resolution_clock::now();
}

double MyTimer::GetElapsedTimeInSecond() const
{
    return GetElapsedTimeInMicroSec() * 0.000001;
}

double MyTimer::GetElapsedTimeInMilliSec() const
{
    return GetElapsedTimeInMicroSec() * 0.001;
}

double MyTimer::GetElapsedTimeInMicroSec() const
{
    return duration_cast<microseconds>(high_resolution_clock::now() - m_begin).count();
}