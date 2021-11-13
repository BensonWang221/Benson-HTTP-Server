#include "RequestFileHandler.h"
#include "http_conn.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

RequestFileHandler::RequestFileHandler(http_conn* conn, const std::string& filePath) :
                                       BaseClass(conn), m_filePath(std::move(filePath))
{

}

RequestFileHandler::~RequestFileHandler()
{
    if (m_filemap)
    {
        munmap(m_filemap, m_fileStat.st_size);
        m_filemap = nullptr;
    }
}

HTTP_CODE RequestFileHandler::GetFileStat()
{
    if (stat(m_filePath.c_str(), &m_fileStat) < 0)
        return NO_RESOURCE;
    
    if (S_ISDIR(m_fileStat.st_mode))
        return BAD_REQUEST;
    
    if (!(m_fileStat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    
    int fd = open(m_filePath.c_str(), O_RDONLY);
    if (fd < 0)
        return INTERNAL_ERROR;
    
    if ((m_filemap = static_cast<char*>(mmap(nullptr, m_fileStat.st_size, PROT_READ,
                                        MAP_PRIVATE, fd, 0))) == MAP_FAILED)
        return INTERNAL_ERROR;

    close(fd);
    return FILE_REQUEST;
}

HTTP_CODE RequestFileHandler::HandleRequest()
{
    HTTP_CODE retcode = GetFileStat();
    if (retcode == FILE_REQUEST)
    {
        size_t pos = m_filePath.rfind('.');
        if (pos == string::npos)
            return INTERNAL_ERROR;
            
        auto postfix = m_filePath.substr(pos + 1);
        string contentType;
        if (postfix == "jpg" || postfix == "gif" || postfix == "png")
            contentType = "image/" + postfix;
        else if (postfix == "zip")
            contentType = "application/zip";
        else if (postfix == "html" || postfix == "css")
            contentType = "text/" + postfix;
        m_conn->AddHeaders("Content-Type", contentType);

        if (!m_conn->SendResponse(200, "OK", m_filemap, m_fileStat.st_size))
            retcode = INTERNAL_ERROR;
    }
    return retcode;
}