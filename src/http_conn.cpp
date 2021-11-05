#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "http_conn.h"

using namespace std;
namespace
{
    // 响应的titles
    const string OK200("OK");
    const string ERROR400("Bad Request");
    const string ERROR400FORM("Your request has bad syntax or is inherently impossible to satisfy.\n");
    const string ERROR403("Forbidden");
    const string ERROR403FORM("You have no permission to get this resource from this server.\n");
    const string ERROR404("Not Found");
    const string ERROR404FORM("The requested resource was not found on this server.\n");
    const string ERROR500("Internal Error");
    const string ERROR500FORM("There was an unusual problem serving the requested resource.\n");

    // 根目录
    const string RootDir("/var/www/html");

    int SetNonblocking(int fd);

    void Addfd(int epollfd, int fd, bool oneshot);

    void Removefd(int epollfd, int fd);

    void Modifyfd(int epollfd, int fd, int ev);
}

int http_conn::epollfd = -1;
int http_conn::userCount = 0;

http_conn::http_conn()
{
    bzero(&m_address, sizeof(m_address));
    bzero(m_readbuf, sizeof(m_readbuf));
    bzero(m_writebuf, sizeof(m_writebuf));
}

void http_conn::Init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    memcpy(&m_address, &addr, sizeof(sockaddr_in));

    // 避免TIME_WAIT状态，实际应用应该去掉
#ifdef TEST
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    Addfd(epollfd, sockfd, true);
    ++userCount;
}

void http_conn::CloseConnection(bool realClose)
{
    if (realClose && m_sockfd != -1)
    {
        Removefd(epollfd, m_sockfd);
        --userCount;
        m_sockfd = -1;
    }
}

bool http_conn::Read()
{
    if (m_sockfd == -1 || m_readPos > READ_BUFFER_SIZE)
        return false;

    int nbytes;
    // 循环读取
    while (true)
    {
        if ((nbytes = recv(m_sockfd, m_readbuf + m_readPos,
                           READ_BUFFER_SIZE - m_readPos, 0)) == -1)
        {
            // 没有数据了
            if (errno == EAGAIN || EWOULDBLOCK)
                break;
            else
                return false;
        }
        else if (nbytes == 0)
            return false;
        m_readPos += nbytes;
    }
    return true;
}

http_conn::LINE_STATUS http_conn::ParseLine()
{
    char temp;
    for (; m_checkedPos < m_readPos; ++m_checkedPos)
    {
        temp = m_readbuf[m_checkedPos];
        if (temp == '\r')
        {
            if (m_checkedPos == (m_readPos - 1))
                return LINE_OPEN;
            if (m_readbuf[m_checkedPos + 1] == '\n')
            {
                m_readbuf[m_checkedPos++] = '\0';
                m_readbuf[m_checkedPos++] = '\0';
                return LINE_OK;
            }
        }
        else if (temp == '\n')
        {
            if (m_checkedPos > 1 && m_readbuf[m_checkedPos - 1] == '\r')
            {
                m_readbuf[m_checkedPos - 1] = '\0';
                m_readbuf[m_checkedPos++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::ParseRequestLine(char *text)
{
    m_url = strpbrk(text, " \t");
    if (!m_url)
        return BAD_REQUEST;
    *(m_url++) = '\0';

    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
        m_method = POST;
    else if (strcasecmp(method, "PUT") == 0)
        m_method = PUT;
    else if (strcasecmp(method, "DELETE") == 0)
        m_method = DELETE;
    else
        return BAD_REQUEST;

    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *(m_version++) = '\0';
    m_version += strspn(m_version, " \t");
    // 只支持http 1.1
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (!m_url)
        return BAD_REQUEST;

    m_checkState = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::ParseHeaders(char *text)
{
    // 空行，说明首部字段解析完毕
    if (text[0] == '\0')
    {
        // 有正文主体
        if (m_contentLength != 0)
        {
            m_checkState = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }

    if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
            m_linger = true;

        m_headers.emplace(string("Connection"), string(text));
    }
    else if (strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_contentLength = atoi(text);
        m_headers.emplace(string("Content-Length"), string(text));
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
        m_headers.emplace(string("Host"), string(text));
    }
    else
    {
        // 这里有待商榷
        string temp = text;
        size_t pos = temp.find(':');
        m_headers.emplace(temp.substr(0, pos), temp.substr(pos + 1));
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::ParseContent(char *text)
{
    m_body = text;
    // 正文已全部获得
    if (m_readPos >= (m_contentLength + m_checkedPos))
    {
        text[m_contentLength] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::ProcessRead()
{
    LINE_STATUS lineStatus = LINE_OK;
    HTTP_CODE result = NO_REQUEST;
    char* text = nullptr;

    // 该解析请求正文后就不需要parse line了
}

namespace
{
    int SetNonblocking(int fd)
    {
        int oldFlag = fcntl(fd, F_GETFL);
        int newFlag = oldFlag |= O_NONBLOCK;
        fcntl(fd, F_SETFL, newFlag);
        return oldFlag;
    }

    void Addfd(int epollfd, int fd, bool oneshot)
    {
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        if (oneshot)
            event.events |= EPOLLONESHOT;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
        SetNonblocking(fd);
    }

    void Removefd(int epollfd, int fd)
    {
        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
    }

    void Modifyfd(int epollfd, int fd, int ev)
    {
        epoll_event event;
        event.data.fd = fd;
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
    }
}
