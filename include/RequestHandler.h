#ifndef REQUEST_HANDLER_INCLUDED
#define REQUEST_HANDLER_INCLUDED

#include "Httpdef.h"

class http_conn;

class RequestHandler
{
public:
    RequestHandler(http_conn* conn) : m_conn(conn) {}

    virtual ~RequestHandler() {}

    virtual HTTP_CODE HandleRequest() = 0;

protected:
    http_conn* m_conn = nullptr;
};

#endif