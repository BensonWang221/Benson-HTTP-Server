#include "libevent_http_server.h"
#include <atomic>
#include <thread>
#include <iostream>
#include <stdio.h>
#include "Timer.h"

std::atomic_int g_count { 0 };

namespace{
    void MessagePerSec()
    {
        static MyTimer timer;
        auto timePeriod = timer.GetElapsedTimeInSecond();

        if (timePeriod >= 1.0 && g_count != 0)
        {
            //printf("message per second: %lf", g_count / timePeriod);
            std::cout << "message per second: " << g_count.load() / timePeriod << std::endl;
            timer.Update();
            g_count = 0;
        }
    }
}

int main()
{
    LibeventServer server;
    std::thread t(std::mem_fun(&LibeventServer::Run), &server);

    while(true)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        MessagePerSec();
    }
    t.join();

    return 0;
}