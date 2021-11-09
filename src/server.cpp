#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cassert>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include "server.h"

using namespace std;

extern void Addfd(int epollfd, int fd, bool oneshot);
extern void Removefd(int epollfd, int fd);

namespace
{
    void AddSig(int sig, void(handler)(int), bool restart = true);

    void StopSignal(int signal);

    // 统一事件源，通过信号使server stop
    int pipefd[2];

    const int MaxEventsNumber = 10000;
}

HttpServer::HttpServer() : m_ip("127.0.0.1"), m_port(8080)
{
}

HttpServer::HttpServer(const string& ip, const short port): m_ip(move(ip)), m_port(port)
{
}

bool HttpServer::Init()
{
    AddSig(SIGPIPE, SIG_IGN);

    // 统一事件源，处理SIGINT
    AddSig(SIGINT, StopSignal);
    
    // 初始化线程池
    m_threadPool = unique_ptr<ThreadPool>(new ThreadPool());

    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    // SO_LINGER,控制关闭时未发送完的数据的行为
    struct linger temp{0, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &temp, sizeof(temp));

    sockaddr_in address;
    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, m_ip.c_str(), &address.sin_addr);
    address.sin_port = htons(m_port);
    int ret;
    ret = bind(m_listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);

    ret = listen(m_listenfd, 50);
    assert(ret != -1);

    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    Addfd(m_epollfd, m_listenfd, false);
    http_conn::epollfd = m_epollfd;
}

void HttpServer::Run()
{
    Init();
    int activeNum = 0;
    epoll_event events[MaxEventsNumber];
    while(m_running)
    {
        activeNum = epoll_wait(m_epollfd, events, MaxEventsNumber, -1);
        // 统一事件源，判断errno非EINTR即出错
        if (activeNum < 0 && errno != EINTR)
        {
            printf("epoll wait error\n");
            break;
        }

        for (int i = 0; i < activeNum; ++i)
        {
            
        }
    }
}

void HttpServer::Stop()
{
    m_running = false;
    m_stopSem.Wait();
}

HttpServer::~HttpServer()
{
    if (m_running)
        Stop();
    if (m_listenfd != -1)
        close(m_listenfd);
    if (m_epollfd != -1)
        close(m_epollfd);
}

namespace
{
    void AddSig(int sig, void(handler)(int), bool restart)
    {
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sigfillset(&sa.sa_mask);
        sa.sa_handler = handler;
        if (restart)
            sa.sa_flags != SA_RESTART;
        assert(sigaction(sig, &sa, nullptr) != -1);
    }

    void StopSignal(int signal)
    {
        int saveError = errno;
        char msg = signal;
        // 将信号值写入管道，通知主循环
        send(pipefd[1], &msg, 1, 0);
        errno = saveError;
    }
}