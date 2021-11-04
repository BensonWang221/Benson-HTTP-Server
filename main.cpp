#include "ThreadPool.h"

int main()
{
    ThreadPool threadpool;
    std::thread thd1([&threadpool]()
    {
        for (int i = 0; i < 10; ++i)
        {
            auto thdID = std::this_thread::get_id();
            threadpool.AddTask([thdID]()
            {
                std::cout << "线程1的线程ID: " << thdID << std::endl;
            });
        }
    });

    std::thread thd2([&threadpool]()
    {
        for (int i = 0; i < 10; ++i)
        {
            auto thdID = std::this_thread::get_id();
            threadpool.AddTask([thdID]()
            {
                std::cout << "线程2的线程ID: " << thdID << std::endl;
            });
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    getchar();
    threadpool.Stop();
    thd1.join();
    thd2.join();

    return 0;
}