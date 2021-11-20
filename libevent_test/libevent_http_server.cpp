#include "libevent_http_server.h"
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <atomic>
#include <functional>

using namespace std;

extern atomic_int g_count;

namespace 
{
    const string RootDir("/www/html");

    void HttpDoRequest(struct evhttp_request* request);

    void HttpCB(struct evhttp_request* request, void* arg)
    {
        auto server = static_cast<LibeventServer*>(arg);
        ThreadPool::Task func = bind(HttpDoRequest, request);
        server->AddTask(func);
    }

    void HttpDoRequest(struct evhttp_request* request)
    {
        string uri = evhttp_request_get_uri(request);
        size_t pos;
        if ((pos = uri.find("http://127.0.0.1")) != string::npos)
            uri = uri.substr(16);
        string filepath = RootDir;
        filepath += uri;
        
        if (uri == "/")
            filepath += "index.html";

        struct stat st; 
        stat(filepath.c_str(), &st);
        int size = st.st_size;
        int fd = open(filepath.c_str(), O_RDONLY);
        char* p = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        evbuffer* output = evhttp_request_get_output_buffer(request);
        evbuffer_add(output, p, size);
        close(fd);
        evhttp_send_reply(request, HTTP_OK, "OK", output);
        munmap(p, size);
        ++g_count;
    }
}

LibeventServer::LibeventServer()
{
    m_base = event_base_new();
    m_evh = evhttp_new(m_base);
    m_threadPool = new ThreadPool();
}

void LibeventServer::Run()
{
    evhttp_bind_socket(m_evh, "127.0.0.1", 8080);
    evhttp_set_gencb(m_evh, HttpCB, this);

    event_base_dispatch(m_base);
}

void LibeventServer::AddTask(ThreadPool::Task t)
{
    this->m_threadPool->AddTask(t);
}