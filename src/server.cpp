#include <unistd.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cassert>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include "server.h"
#include "Timer.h"

using namespace std;

extern void Addfd(int epollfd, int fd, bool oneshot);
extern void Removefd(int epollfd, int fd);

namespace
{
    void AddSig(int sig, void(handler)(int), bool restart = true);

    void StopSignal(int signal);

#ifdef TEST
    atomic_int messageCount{0};
#endif

    // 统一事件源，通过信号使server stop
    //int pipefd[2];
    int sfd;

    const int MaxEventsNumber = 10000;

    const int MaxFd = 65536;
}

HttpServer::HttpServer() : m_ip("127.0.0.1"), m_port(8080)
{
}

HttpServer::HttpServer(const string &ip, const short port) : m_ip(move(ip)), m_port(port)
{
}

bool HttpServer::Init()
{
    //AddSig(SIGPIPE, SIG_IGN);

    // 统一事件源，处理SIGINT
    //AddSig(SIGINT, StopSignal);
    // Use signalfd instead
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGPIPE);
    sigaddset(&sigset, SIGINT);
    assert(sigprocmask(SIG_BLOCK, &sigset, nullptr) != -1);
    sfd = signalfd(-1, &sigset, SFD_NONBLOCK);
    assert(sfd != -1);

    // 初始化线程池
    m_threadPool = unique_ptr<ThreadPool>(new ThreadPool());

    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    // SO_LINGER,控制关闭时未发送完的数据的行为
    struct linger temp
    {
        0, 1
    };
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &temp, sizeof(temp));

    sockaddr_in address;
    memset(&address, '\0', sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, m_ip.c_str(), &address.sin_addr);
    address.sin_port = htons(m_port);
    int ret;
    //ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
    //assert(ret != -1);
    int reuse = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    ret = bind(m_listenfd, (sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(m_listenfd, 50);
    assert(ret != -1);

    try
    {
        m_clients = new http_conn[MaxFd];
    }
    catch (...)
    {
        std::cout << "new error\n"
                  << std::endl;
        return false;
    }

    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    Addfd(m_epollfd, m_listenfd, false);
    Addfd(m_epollfd, sfd, false);
    http_conn::epollfd = m_epollfd;
    return true;
}

void HttpServer::Run()
{
    printf("Starting server...\n");
    bool ret = Init();
    if (!ret)
    {
        std::cout << "Server init error." << std::endl;
        exit(1);
    }
    int activeNum = 0;
    epoll_event events[MaxEventsNumber];
    m_running = true;
    while (m_running)
    {
#ifdef TEST
        MessagePerSec();
#endif
        activeNum = epoll_wait(m_epollfd, events, MaxEventsNumber, -1);
        // 统一事件源，判断errno非EINTR即出错
        if (activeNum < 0 && errno != EINTR)
        {
            printf("epoll wait error\n");
            break;
        }

        for (int i = 0; i < activeNum; ++i)
        {
            int fd = events[i].data.fd;
            if (fd == m_listenfd)
            {
                // listenfd也要循环读取
                int connfd;
                sockaddr_in connAddress;
                while (true)
                {
                    socklen_t len;
                    connfd = accept(m_listenfd, (sockaddr *)&connAddress, &len);
                    if (connfd == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        else if (errno == EINTR)
                            continue;
                        else
                        {
                            perror("accept error.");
                            printf("errno = %d\n", errno);
                            exit(1);
                        }
                    }
                    if (http_conn::userCount >= MaxFd)
                    {
                        send(connfd, "Full", 4, 0);
                        close(connfd);
                        break;
                    }
                    m_clients[connfd].Init(connfd, connAddress);
                }
            }
            // handle the signal
            else if (fd == sfd)
            {
                signalfd_siginfo fdinfo;
                int size = 0;
                bool stopServer = false;
                while ((size = read(sfd, &fdinfo, sizeof(fdinfo))) == sizeof(fdinfo))
                {
                    if (fdinfo.ssi_signo == SIGINT)
                    {
                        cout << "Caught SIGINT, going to close the server." << endl;
                        stopServer == true;
                        m_running = false;
                        break;
                    }
                    else if (fdinfo.ssi_signo == SIGPIPE)
                        continue;
                    else
                    {
                        cout << "unexpected signal" << endl;
                        continue;
                    }
                }
                if (stopServer)
                    break;

            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
                m_clients[fd].CloseConnection();
            else if (events[i].events & EPOLLIN)
            {
                if (m_clients[fd].Read())
                {
                    http_conn &conn = m_clients[fd];
                    auto task = [&conn]()
                    { conn(); };
                    m_threadPool->AddTask(task);
                }
                else
                    m_clients[fd].CloseConnection();
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!m_clients[fd].Write())
                    m_clients[fd].CloseConnection();
#ifdef TEST
                messageCount++;
#endif
            }
        }
    }
    m_threadPool->Stop();
    m_stopSem.Post();
    printf("Closing server...\n");

    close(m_epollfd);
    close(m_listenfd);
    delete[] m_clients;
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

#ifdef TEST
void HttpServer::MessagePerSec()
{
    static MyTimer timer;
    auto timePeriod = timer.GetElapsedTimeInSecond();

    if (timePeriod >= 1.0)
    {
        printf("message per second: %lf, tasks in queue: %lu\n", messageCount / timePeriod,
               m_threadPool->GetTasksInQueue());
        timer.Update();
        messageCount = 0;
    }
}
#endif

namespace
{
   // void AddSig(int sig, void(handler)(int), bool restart)
   // {
   //     struct sigaction sa;
   //     memset(&sa, '\0', sizeof(sa));
   //     sigfillset(&sa.sa_mask);
   //     sa.sa_handler = handler;
   //     if (restart)
   //         sa.sa_flags |= SA_RESTART;
   //     assert(sigaction(sig, &sa, nullptr) != -1);
   // }

   // void StopSignal(int signal)
   // {
   //     int saveError = errno;
   //     char msg = signal;
   //     // 将信号值写入管道，通知主循环
   //     send(pipefd[1], &msg, 1, 0);
   //     errno = saveError;
   // }

}