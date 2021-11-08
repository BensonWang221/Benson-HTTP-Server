#ifndef REQUEST_HANDLER_INCLUDED
#define REQUEST_HANDLER_INCLUDED

class http_conn;

class RequestHandler
{
public:
    RequestHandler(http_conn* conn) : m_conn(conn) {}

    virtual ~RequestHandler() {}

    virtual bool HandleRequest() = 0;

protected:
    http_conn* m_conn = nullptr;
};

#endif