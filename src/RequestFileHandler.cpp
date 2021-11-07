#include "RequestFileHandler.h"
#include "http_conn.h"
#include <sys/mman.h>

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

void RequestFileHandler::HandleRequest()
{
    
}