#ifndef SERVER_INCLUDED
#define SERVER_INCLUDED

#include <string>
#include <memory>
#include <unordered_map>
#include "ThreadPool.h"
#include "http_conn.h"
#include "locker.h"

class HttpServer
{
public:
    HttpServer();
    HttpServer(const std::string& ip, const short port);

    void Run();
    
    void Stop();

private:
    bool Init();

private:
    std::string m_ip;
    short m_port = -1;
    std::unique_ptr<ThreadPool> m_threadPool;
    std::unordered_map<int, http_conn> m_clients;
    Sem m_stopSem;
    
    int m_epollfd = -1;
    int m_listenfd = -1;
    bool m_running = false;
};

#endif