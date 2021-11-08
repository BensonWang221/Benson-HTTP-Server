#ifndef REQUEST_FILE_HANDLER_INCLUDED
#define REQUEST_FILE_HANDLER_INCLUDED

#include "RequestHandler.h"
#include "Httpdef.h"
#include <fcntl.h>
#include <string>

class RequestFileHandler : public RequestHandler
{
public:
    using BaseClass = RequestHandler;
    
    RequestFileHandler(http_conn* conn, const std::string& filePath);

    ~RequestFileHandler();

    bool HandleRequest() override;

private:
    // 分析文件属性
    HTTP_CODE GetFileStat();

    // 释放资源
    void Unmap();

private:
    std::string m_filePath;

    // mmap地址
    char* m_filemap = nullptr;
    struct stat m_fileStat;
};

#endif