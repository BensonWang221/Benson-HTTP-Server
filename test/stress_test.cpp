#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <netinet/in.h>
#include <string>
#include <thread>

using namespace std;

namespace
{
    string message("GET http://127.0.0.1/index.html HTTP/1.1\r\nConnection: keep-alive\r\n\r\nxxxxxxxxxxxxxxxxxxxx");
}

int SetNonblocking(int fd)
{
    int oldFlag = fcntl(fd, F_GETFL);
    int newFlag = oldFlag | O_NONBLOCK;
    fcntl(fd, F_SETFL, newFlag);
    return oldFlag;
}

void addfd(int epollfd, int fd)
{
    epoll_event ev;
    ev.events = EPOLLOUT | EPOLLET | EPOLLERR;
    ev.data.fd = fd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    SetNonblocking(fd);
}

bool WriteN(int sockfd, const string& msg = message)
{
    int haveWritten = 0;
    const char* data = msg.c_str();
    int toWrite = msg.size();
    while(true)
    {
        haveWritten = send(sockfd, data, toWrite, 0);
        if (haveWritten <= 0)
            return false;
        
        toWrite -= haveWritten;
        data = data + haveWritten;
        if (toWrite <= 0)
            return true;
    }
}

bool Read(int sockfd, char* buf, int len)
{
    int haveRead = recv(sockfd, buf, len, 0);
    if (haveRead <= 0)
        return false;
    
    return true;
}

void Connect(int epollfd, int num)
{
    string ip = "127.0.0.1";
    short port = 8080;
    int ret = 0;
    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &address.sin_addr.s_addr);

    for (int i = 0; i < num; ++i)
    {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
            continue;
        if (connect(sockfd, (sockaddr*)&address, sizeof(address)) == 0)
        {
            addfd(epollfd, sockfd);
        }
    }
}

void CloseConn(int epollfd, int sockfd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
    close(sockfd);
}

int main(int argc, char* argv[])
{
    int epollfd = epoll_create(5);

    thread threads[2];
    for (auto& thd : threads)
        thd = thread(Connect, epollfd, 1000);
    
    char buf[1024];
    epoll_event events[10000];
    while(true)
    {
        int fds = epoll_wait(epollfd, events, 10000, -1);
        for (int i = 0; i < fds; ++i)
        {
            int sockfd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {
                if (!Read(sockfd, buf, 1024))
                    CloseConn(epollfd, sockfd);

                epoll_event ev;
                ev.events = EPOLLOUT | EPOLLET | EPOLLERR;
                ev.data.fd = sockfd;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev);
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!WriteN(sockfd))
                    CloseConn(epollfd, sockfd);
                epoll_event ev;
                ev.events = EPOLLIN | EPOLLET | EPOLLERR;
                ev.data.fd = sockfd;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev);
            }
            else if (events[i].events & EPOLLERR)
            {
                std::cout << "event error" << std::endl;
            }
        }
    }
    for (auto& thd : threads)
        thd.join();
    
    return 0;
}
