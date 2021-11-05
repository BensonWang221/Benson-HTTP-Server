#ifndef HTTP_CONNECTION_INCLUDED
#define HTTP_CONNECTION_INCLUDED 

#include <netinet/in.h>
#include <map>
#include <string>
#include <fcntl.h>

class http_conn
{
public:
    // 文件名最大长度
    static const int FILENAME_LEN = 200;
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

    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, 
                     NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,
                     INTERNAL_ERROR, CLOSED_CONNECTION };
    
    static int epollfd;
    static int userCount;

public:
    // default constructor
    http_conn();
    // destructor
    virtual ~http_conn() {}

    // 初始化新连接
    void Init(int sockfd, const sockaddr_in& addr);

    // 关闭连接
    void CloseConnection(bool realClose = true);

    // 处理请求
    void operator()();

    // 非阻塞读
    bool Read();

    // 非阻塞写
    bool Write();

    // 获取首部字段
    // 用右值引用或const引用接收
    const std::map<std::string, std::string> GetHeaders() const;
protected:
    // 当收到完整的请求报文后做的处理
    // 根据需要，重载此函数做相应处理
    virtual HTTP_CODE DoRequest();

private:
    // 解析请求
    HTTP_CODE ProcessRead();

    // 添加HTTP应答
    bool ProcessWrite(HTTP_CODE code);

    // 解析请求函数
    HTTP_CODE ParseRequestLine(char* text);
    HTTP_CODE ParseHeaders(char* text);
    HTTP_CODE ParseContent(char* text);
    char* GetLine() { return m_readbuf + m_curlinePos; }
    LINE_STATUS ParseLine();

    // 应答函数
    void Unmap();
    bool AddResponse(const char* format, ...);
    bool AddContent(const char* content);
    bool AddStatusLine(int status, const std::string& title);
    bool AddHeaders(int contentLength);
    bool AddContentLength(int contentLength);
    bool AddLinger();
    bool AddBlankLine();

private:
    // 对方的socket fd和地址
    int m_sockfd = -1;
    sockaddr_in m_address;

    // 读缓冲区
    char m_readbuf[READ_BUFFER_SIZE];
    // 已经读到的数据长度
    int m_readPos = -1;
    // 正在解析的数据位置
    int m_checkedPos = -1;
    // 当前解析行的起始数据位置
    int m_curlinePos = -1;

    // 写缓冲区
    char m_writebuf[WRITE_BUFFER_SIZE];
    // 待发送字节数
    int m_writeBytes;

    // 主状态机当前状态
    CHECK_STATE m_checkState = CHECK_STATE_REQUESTLINE;
    // 请求方法
    METHOD m_method;
    // 首部字段
    std::map<std::string, std::string> m_headers;

    // url
    char* m_url;
    // 请求的文件路径
    char* m_filePath;
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

    // mmap的位置
    char* m_filemap = nullptr;
    // 目标文件状态
    struct stat m_fileStat;

    // writev
    iovec m_iv[2];
    int m_iv_count; 
};

#endif