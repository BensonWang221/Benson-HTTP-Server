#include <unistd.h>
#include <stdarg.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <memory>
#include <iostream>
#include "http_conn.h"
#include "RequestFileHandler.h"

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

}
inline int SetNonblocking(int fd);

inline void Addfd(int epollfd, int fd, bool oneshot);

inline void Removefd(int epollfd, int fd);

inline void Modifyfd(int epollfd, int fd, int ev);

int http_conn::epollfd = -1;
int http_conn::userCount = 0;

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
    Init();
}

void http_conn::Init()
{
    m_checkState = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = nullptr;
    m_version = m_host = m_body = nullptr;
    m_contentLength = 0;
    m_checkedPos = m_curlinePos = m_readPos = 0;
    m_writeBytes = m_statusBytes = 0;
    m_ivCount = 1;
    bzero(m_readbuf, sizeof(m_readbuf));
    bzero(m_writebuf, sizeof(m_writebuf));
    bzero(&m_address, sizeof(m_address));
    bzero(m_readbuf, sizeof(m_readbuf));
    bzero(m_writebuf, sizeof(m_writebuf));
    m_handler.reset();
}

void http_conn::CloseConnection(bool realClose)
{
    if (realClose && m_sockfd != -1)
    {
        Removefd(epollfd, m_sockfd);
        --userCount;
        m_sockfd = -1;
        if (m_handler)
            m_handler.reset();
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

HTTP_CODE http_conn::ParseRequestLine(char *text)
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

HTTP_CODE http_conn::ParseHeaders(char *text)
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

HTTP_CODE http_conn::ParseContent(char *text)
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

HTTP_CODE http_conn::ProcessRead()
{
    printf("http_conn::ProcessRead..\n");
    LINE_STATUS lineStatus = LINE_OK;
    HTTP_CODE result = NO_REQUEST;
    char *text = nullptr;

    // 该解析请求正文后就不需要parse line了
    // check content时判断linestatus是因为content内容没有全部收到时也不再循环
    while ((m_checkState == CHECK_STATE_CONTENT && lineStatus == LINE_OK) || ((lineStatus = ParseLine()) == LINE_OK))
    {
        text = GetLine();
        std::cout << text << std::endl;
        m_curlinePos = m_checkedPos;
        switch (m_checkState)
        {
        case CHECK_STATE_REQUESTLINE:
            result = ParseRequestLine(text);
            if (result == BAD_REQUEST)
                return BAD_REQUEST;
            break;

        case CHECK_STATE_HEADER:
            result = ParseHeaders(text);
            if (result == BAD_REQUEST)
                return BAD_REQUEST;
            else if (result == GET_REQUEST)
                return DoRequest();
            break;

        case CHECK_STATE_CONTENT:
            result = ParseContent(text);
            if (result == GET_REQUEST)
                return DoRequest();
            // content不完整，跳出大循环
            lineStatus = LINE_OPEN;
            break;

        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HTTP_CODE http_conn::DoRequest()
{
    // 先只做请求文件的处理，api后续再做
    string filepath = RootDir + (strcmp(m_url, "/") == 0 ? "/index.html" : m_url);
    if (!m_handler)
        m_handler = unique_ptr<RequestFileHandler>(new RequestFileHandler(this, std::move(filepath)));

    return m_handler->HandleRequest();
}

bool http_conn::SendErrorCode(HTTP_CODE code)
{
    switch (code)
    {
    case INTERNAL_ERROR:
        return AddErrorTitleForm(500, ERROR500, ERROR500FORM.c_str());
        break;
    case BAD_REQUEST:
        return AddErrorTitleForm(400, ERROR400, ERROR400FORM.c_str());
        break;
    case NO_RESOURCE:
        return AddErrorTitleForm(404, ERROR404, ERROR404FORM.c_str());
        break;
    case FORBIDDEN_REQUEST:
        return AddErrorTitleForm(403, ERROR403, ERROR403FORM.c_str());
        break;
    default:
        return false;
    }
    return false;
}

bool http_conn::AddErrorTitleForm(const int code, const string &title, const string &form)
{
    bool ret = (AddStatusLine(code, title) &&
                AddHeaders("Content-Length", to_string(form.size())) &&
                AddLinger() &&
                AddBlankLine() &&
                AddResponse("%s", form.c_str()));
}

bool http_conn::AddResponse(const char *format, ...)
{
    if (m_writeBytes >= WRITE_BUFFER_SIZE)
        return false;
    va_list argList;
    va_start(argList, format);
    int len = vsnprintf(m_writebuf + m_writeBytes + offset, WRITE_BUFFER_SIZE - m_writeBytes - 1,
                        format, argList);

    // 要留出\r\n的空行
    if (len >= WRITE_BUFFER_SIZE - m_writeBytes - 3)
        return false;
    m_writeBytes += len;
    va_end(argList);
    return true;
}

bool http_conn::AddStatusLine(int status, const string &title)
{
    string statusLine = (to_string(status) + " HTTP/1.1 ") + title + "\r\n";
    m_statusBytes = statusLine.size();
    memcpy(m_writebuf + offset - m_statusBytes, statusLine.c_str(), m_statusBytes);
    return true;
}

bool http_conn::AddHeaders(const string &key, const string &value)
{
    if (AddResponse("%s: %s\r\n", key.c_str(), value.c_str()))
    {
        // 每次加header之后，就把后面两个位置设置为\r\n
        m_writebuf[m_writeBytes + offset] = '\r';
        m_writebuf[m_writeBytes + 1 + offset] = '\n';
        return true;
    }
    return false;
}

bool http_conn::AddLinger()
{
    return AddHeaders("Connection", m_linger ? "keep-alive" : "close");
}

bool http_conn::AddBlankLine()
{
    AddResponse("%s", "\r\n");
}

bool http_conn::AddResponseBody(const char *text, size_t size)
{
    AddHeaders("Content-Length", to_string(size));
}

bool http_conn::SendResponse(int code, const string &title, const char *content, size_t len)
{
    bool ret = (AddStatusLine(code, title) &&
                AddLinger() &&
                AddResponseBody(content, len));
    if (ret)
    {
        m_iv[0].iov_base = m_writebuf + offset - m_statusBytes;
        m_iv[0].iov_len = m_statusBytes + m_writeBytes + 2;
        m_iv[1].iov_base = const_cast<char*>(content);
        m_iv[1].iov_len = len;
        m_ivCount = 2;
        m_writeBytes = m_writeBytes + m_statusBytes + len + 2;
    }   
    return ret;
}

bool http_conn::ProcessWrite(HTTP_CODE code)
{
    switch (code)
    {
    case INTERNAL_ERROR:
        if (!AddErrorTitleForm(500, ERROR500, ERROR500FORM))
            return false;
        break;
    case BAD_REQUEST:
        if (!AddErrorTitleForm(400, ERROR400, ERROR400FORM))
            return false;
        break;
    case NO_RESOURCE:
        if (!AddErrorTitleForm(404, ERROR404, ERROR404FORM))
            return false;
        break;
    case FORBIDDEN_REQUEST:
        if (!AddErrorTitleForm(403, ERROR403, ERROR403FORM))
            return false;
        break;
    case FILE_REQUEST:
        return true;
    default:
        return false;
    }
    m_iv[0].iov_base = m_writebuf + offset - m_statusBytes;
    // 将\r\n加进去
    m_iv[0].iov_len = m_statusBytes + m_writeBytes + 2;
    m_ivCount = 1;
    m_writeBytes += (m_statusBytes + 2);
    return true;
}

bool http_conn::Write()
{
    int ret = 0;
    int bytesToSend = m_writeBytes,
        bytesHaveSent = 0;

    if (bytesToSend == 0)
    {
        Modifyfd(epollfd, m_sockfd, EPOLLIN);
        Init();
        return true;
    }

    while (true)
    {
        if ((ret = writev(m_sockfd, m_iv, m_ivCount)) < 0)
        {
            // 当写缓冲区已满，等待下一轮EPOLLOUT事件
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                Modifyfd(epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            m_handler.reset();
            return false;
        }
        bytesToSend -= ret;
        bytesHaveSent += ret;
        if (bytesToSend <= bytesHaveSent)
        {
            // 已经处理完毕，释放handler资源
            m_handler.reset();
            if (m_linger)
            {
                Init();
                Modifyfd(epollfd, m_sockfd, EPOLLIN);
                return true;
            }
            else
            {
                Modifyfd(epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}

void http_conn::operator()()
{
    printf("http_conn:: operator()..\n");
    HTTP_CODE readRet = ProcessRead();
    if (readRet == NO_REQUEST)
    {
        Modifyfd(epollfd, m_sockfd, EPOLLIN);
        return;
    }
    if (!ProcessWrite(readRet))
        CloseConnection();
    else
        Modifyfd(epollfd, m_sockfd, EPOLLOUT);
}

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
