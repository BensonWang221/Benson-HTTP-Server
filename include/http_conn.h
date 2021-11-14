#ifndef HTTP_CONNECTION_INCLUDED
#define HTTP_CONNECTION_INCLUDED 

#include <netinet/in.h>
#include <map>
#include <string>
#include <fcntl.h>
#include <memory>
#include <vector>
#include "Httpdef.h"
#include "RequestHandler.h"

void Addfd(int epollfd, int fd, bool oneshot);
void Removefd(int epollfd, int fd);

class RequestHandler;

class http_conn
{
public:
    // 读缓冲区大小
    static const int READ_BUFFER_SIZE = 2048;
    // 写缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;

    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE,
                  TRACE, OPTIONS, CONNECT, PATCH };

    // 主状态机状态
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0,
                       CHECK_STATE_HEADER,
                       CHECK_STATE_CONTENT };
    
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

    static int epollfd;
    static int userCount;

    // 为状态行预留的offset
    static const int offset = 128;

public:
    // default constructor
    http_conn() = default;
    // destructor
    virtual ~http_conn() {}

    // 初始化新连接
    void Init(int sockfd, const sockaddr_in& addr);

    // 关闭连接
    void CloseConnection(bool realClose = true);

    // 非阻塞读
    bool Read();

    // 非阻塞写
    bool Write();

    // 入口函数
    void operator()();

    bool AddHeaders(const std::string& key, const std::string& value);

    bool AddStatusLine(int status, const std::string& title);

    // 手动调用
    bool SendResponse(int code, const std::string& title, const char* content, size_t len);

    // 返回错误
    bool SendErrorCode(HTTP_CODE code);

    // 获取首部字段
    // 用右值引用或const引用接收
    const std::map<std::string, std::string> GetHeaders() const;
protected:  

private:
    void Init();

    // 解析请求
    HTTP_CODE ProcessRead();

    // 解析请求函数
    HTTP_CODE ParseRequestLine(char* text);
    HTTP_CODE ParseHeaders(char* text);
    HTTP_CODE ParseContent(char* text);
    char* GetLine() { return m_readbuf + m_curlinePos; }
    LINE_STATUS ParseLine();

    HTTP_CODE DoRequest();

    bool ProcessWrite(HTTP_CODE code);

    // 应答函数
    bool AddResponse(const char* format, ...);
    bool AddLinger();
    bool AddBlankLine();
    bool AddResponseBody(const char* text, size_t size);
    bool AddErrorTitleForm(const int code, const std::string& title, const std::string& form);

private:
    std::unique_ptr<RequestHandler> m_handler;

    // 对方的socket fd和地址
    int m_sockfd = -1;
    sockaddr_in m_address;

    // 读缓冲区
    char m_readbuf[READ_BUFFER_SIZE];
    // 已经读到的数据长度
    int m_readPos = 0;
    // 正在解析的数据位置
    int m_checkedPos = 0;
    // 当前解析行的起始数据位置
    int m_curlinePos = 0;

    // 写缓冲区
    char m_writebuf[WRITE_BUFFER_SIZE + offset];
    int m_statusBytes = 0;
    // 首部字段字节数
    int m_writeBytes = 0;

    // 主状态机当前状态
    CHECK_STATE m_checkState = CHECK_STATE_REQUESTLINE;
    // 请求方法
    METHOD m_method;
    // 首部字段
    std::map<std::string, std::string> m_headers;

    // url
    char* m_url;
    // 协议版本号
    char* m_version;
    // 主机名
    char* m_host;
    // 正文主体
    char* m_body;
    // body长度
    int m_contentLength = 0;
    // 是否长连接
    bool m_linger = false;

    // writev
    iovec m_iv[2];
    int m_ivCount = 1; 
};

#endif