
#include "HTTPServer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <assert.h>

#include "socketWrap.h"
#include "IFileAccess.h"

//-------------------------------------------------------------------------
static const int gListeningPort = 15000;
static const int gBufferSize = 10*1024*1024;

//-------------------------------------------------------------------------
typedef enum
{
    METHOD_OPTIONS = 0,
    METHOD_GET,
    METHOD_HEAD,
    METHOD_POST,
    METHOD_PUT,
    METHOD_DELETE,
    METHOD_TRACE,
    METHOD_CONNECT,
    METHOD_UNKNOWN
} Method;

//-------------------------------------------------------------------------
typedef enum
{
    ERR_BAD_REQUEST = 0,
    ERR_NOT_FOUND,
    ERR_BAD_METHOD,
    ERR_UNAVAILABLE,
    ERR_METHOD_NOT_ALLOWED,
    ERR_HOST_NOT_ALLOWED,
    ERR_URL_NOT_ALLOWED,
    ERR_DATE_NOT_ALLOWED
} HTTPError;

//-------------------------------------------------------------------------
static Method getMethod(byte *request)
{
    if (memcmp(request, "OPTIONS ", 8) == 0) return METHOD_OPTIONS;
    else if (memcmp(request, "GET ", 4) == 0) return METHOD_GET;
    else if (memcmp(request, "HEAD ", 5) == 0) return METHOD_HEAD;
    else if (memcmp(request, "POST ", 5) == 0) return METHOD_POST;
    else if (memcmp(request, "PUT ", 4) == 0) return METHOD_PUT;
    else if (memcmp(request, "DELETE ", 7) == 0) return METHOD_DELETE;
    else if (memcmp(request, "TRACE ", 6) == 0) return METHOD_TRACE;
    else if (memcmp(request, "CONNECT ", 7) == 0) return METHOD_CONNECT;
    else return METHOD_UNKNOWN;
}

//-------------------------------------------------------------------------
static int sendRawToSocket(sw_socket *s, byte *src, int len)
{
    int sent = 0;

    while(len > 0)
    {
        int r = sw_send(s, src, len, 0);
        if (r <= 0) return sent;

        sent += r;
        src += r;
        len -= r;
    }

    return sent;
}

//-------------------------------------------------------------------------
static int sendToSocket(sw_socket *s, const char *src)
{
    return sendRawToSocket(s, (byte*)src, strlen(src));
}

//-------------------------------------------------------------------------
static int sendErrorMessage(sw_socket *s, const char *num, const char *reason, const char *name, const char *body)
{
    int err, len = 0;
    err = sendToSocket(s, "HTTP/1.1 ");     if (err <= 0) return err; len += err;
    err = sendToSocket(s, num);             if (err <= 0) return err; len += err;
    err = sendToSocket(s, " ");             if (err <= 0) return err; len += err;
    err = sendToSocket(s, reason);          if (err <= 0) return err; len += err;
    err = sendToSocket(s, "\r\n\r\n");      if (err <= 0) return err; len += err;
    err = sendToSocket(s, "<html><head><title>"); if (err <= 0) return err; len += err;
    err = sendToSocket(s, name);            if (err <= 0) return err; len += err;
    err = sendToSocket(s, "</title></head><body><h1>"); if (err <= 0) return err; len += err;
    err = sendToSocket(s, name);            if (err <= 0) return err; len += err;
    err = sendToSocket(s, "</h1><br><h3>"); if (err <= 0) return err; len += err;
    err = sendToSocket(s, body);            if (err <= 0) return err; len += err;
    err = sendToSocket(s, "</h3></body></html>\n"); if (err <= 0) return err; len += err;
    return len;
}

//-------------------------------------------------------------------------
static int sendError(sw_socket *s, HTTPError errorCode)
{
    switch(errorCode)
    {
        case ERR_BAD_REQUEST:
            return sendErrorMessage(s, "400", "BadRequest",
                                        "400 Bad Request",
                                        "Oups, bad request!");
            break;

        case ERR_BAD_METHOD:
            return sendErrorMessage(s, "501", "MethodNotImplemented",
                                        "501 Method Not Implemented",
                                        "Oups, method not implemented!");
            break;

        case ERR_UNAVAILABLE:
            return sendErrorMessage(s, "503", "ServerUnavailable",
                                        "503 Server Unavailable",
                                        "Oups, server is not available!");
            break;

        case ERR_METHOD_NOT_ALLOWED:
            return sendErrorMessage(s, "401", "UnauthorizedMethod",
                                        "401 Unauthorized Method",
                                        "Oups, method is not allowed!");
            break;

        case ERR_HOST_NOT_ALLOWED:
            return sendErrorMessage(s, "401", "UnauthorizedHost",
                                        "401 Unauthorized Host",
                                        "Oups, host is not allowed!");
            break;

        case ERR_URL_NOT_ALLOWED:
            return sendErrorMessage(s, "401", "UnauthorizedURL",
                                        "401 Unauthorized URL",
                                        "Oups, URL is not allowed!");
            break;

        case ERR_DATE_NOT_ALLOWED:
            return sendErrorMessage(s, "401", "UnauthorizedDate",
                                        "401 Unauthorized Date",
                                        "Oups, Date is not allowed!<br>(Document is too old)");
            break;

        case ERR_NOT_FOUND:
            return sendErrorMessage(s, "404", "NotFound",
                                        "404 Not Found",
                                        "Oups, File not found");
            break;
    }

    return -1;
}

//-------------------------------------------------------------------------
static int sendHead(sw_socket *s, const char *etag, int64 size, bool hasRange, int64 rangeStart, int64 rangeSize)
{
    char contentField[1024], eTagField[1024];
    const char *answer;

    sprintf(eTagField, "ETag: \"%s\"\r\n", etag);

    if (hasRange)
    {
        answer = "HTTP/1.1 206 Partial Content\r\n";
        sprintf(contentField, "Content-Range: bytes %lld-%lld/%lld\r\nContent-Length: %lld\r\n",
                    rangeStart, rangeStart + rangeSize - 1,
                    size, rangeSize);
    }
    else
    {
        answer = "HTTP/1.1 200 OK\r\n";
        sprintf(contentField, "Content-Length: %lld\r\n", size);
    }

    int err, len = 0;
    err = sendToSocket(s, answer);                                          if (err <= 0) return err; len += err;
    err = sendToSocket(s, "Accept-Ranges: bytes\r\n");                      if (err <= 0) return err; len += err;
    err = sendToSocket(s, "Content-Type: text/plain\r\n");                  if (err <= 0) return err; len += err;
    err = sendToSocket(s, contentField);                                    if (err <= 0) return err; len += err;
    err = sendToSocket(s, eTagField);                                       if (err <= 0) return err; len += err;
    err = sendToSocket(s, "\r\n");                                          if (err <= 0) return err; len += err;
    return len;
}

//-------------------------------------------------------------------------
// read up to \r\n\r\n (included) and returns number of bytes read
static int getHeader(sw_socket *s, byte *dst, int maxlen)
{
    int count = 0;
    while(1)
    {
        int r = sw_recv(s, dst, maxlen, 0);
        if (r <= 0) return count;

        count += r;
        dst += r;
        maxlen -= r;

//      fprintf(stderr, "socket: %p bytes: %i\n", s, count);

        if (count >= 4)
        {
            if (memcmp(dst - 4, "\r\n\r\n", 4) == 0)
                break;
        }
    }
    return count;
}

//-------------------------------------------------------------------------
// extract url and range from requestline
static void parseRequest(const char *request, char *url, bool *hasRange, int64 *rangeStart, int64 *rangeLength)
{
    *hasRange = false;
    *rangeStart = 0;
    *rangeLength = 0;

    while(*request && *request != ' ') request++; request++;
    while(*request && *request != ' ') *url++ = *request++;
    while(*request && *request != '\n') request++; request++;

    while(*request)
    {
        int c = sscanf(request, "Range: bytes=%lld-%lld", rangeStart, rangeLength);
        if (c == 2) { *hasRange = true; *rangeLength = *rangeLength - *rangeStart + 1; }
        while(*request && *request != '\n') request++; request++;
    }

    *url = 0;
}


//-------------------------------------------------------------------------
struct _HTTPServer
{
    IFileAccess *dw;
    sw_socket *lst;
    int port;
    pthread_t thread;
};

//-------------------------------------------------------------------------
struct Connection
{
    IFileAccess *dw;
    sw_socket *s;
};

//-------------------------------------------------------------------------
static void serveSocket(Connection *c)
{
    sw_socket *s = c->s;
    IFileAccess *dw = c->dw;
    free(c);

    byte *header;

    char url[1024];
    int length, method;
    bool hasRange;
    int64 rangeStart, rangeLength;

    header = (byte*) malloc(gBufferSize);

    while(1)
    {
//      printf("==== Waiting for request...\n");

        // get the request
        length = getHeader(s, header, gBufferSize - 1);
        if (length <= 0) break;
        header[length] = 0;

        // check kind of request
        method = getMethod(header);
        if (method != METHOD_HEAD && method != METHOD_GET)
        {
            sendError(s, ERR_BAD_METHOD);
            continue;
        }

        // get url, host and port
        parseRequest((char*)header, url, &hasRange, &rangeStart, &rangeLength);
        if (rangeLength > 1024*1024*256) rangeLength = 1024*1024*256;

#if 0
        {
            printf("================================================\n");
            printf("header:\n%s\n", header);
            printf("= = = = = = = = = = = = = = = = = = = = = = = = \n");
            printf("url: %s\n", url);
            if (hasRange) printf("range: %lld - %lld (%lld)\n", rangeStart, rangeStart + rangeLength - 1, rangeLength);
            printf("\n");
        }
#endif

        const char *path = "/dvd.iso";
        if (strcmp(url, path) != 0)
        {
            sendError(s, ERR_NOT_FOUND);
            continue;
        }

        int64 fileSize = dw->size();
        int64 requestSize = (hasRange) ? rangeLength : fileSize;
        byte *data = NULL;

        if (method == METHOD_GET)
        {
            data = dw->lock(rangeStart, requestSize);
            if (!data)
            {
                fprintf(stderr, "Data read error\n");
                sendError(s, ERR_NOT_FOUND);
                continue;
            }
        }

        sendHead(s, path, fileSize, hasRange, rangeStart, rangeLength);
        if (data)
        {
            sendRawToSocket(s, data, requestSize);
            dw->unlock();
        }
    }

    sw_close(s);
    free(header);
}

//-------------------------------------------------------------------------
static void acceptSocket(HTTPServer *server)
{
    while(server->lst)
    {
        sw_socket *s = sw_accept(server->lst);
        if (!s) break;

        if (!server->lst) break;

        Connection *c = (Connection *) malloc ( sizeof(Connection) );
        c->s = s;
        c->dw = server->dw;

        pthread_t thread;
        pthread_create(&thread, 0, (void *(*)(void*))serveSocket, c);
    }
}

//-------------------------------------------------------------------------
HTTPServer* createServer(const char *device, std::vector<FMFileInfo> fileList)
{
    fprintf(stderr, "createServer device: %s\n", (device ? device : "null"));
    if (!device) return NULL;

    //----------------
    IFileAccess *dw = IFileAccess::createCached(IFileAccess::createCSS(device, fileList));
    fprintf(stderr, "createServer IFileAccess: %p\n", dw);
    if (!dw) return NULL;

    //----------------
    sw_socket *lst;
    int port = gListeningPort;
    int tryCount = 100;
    while(tryCount > 0)
    {
        lst = sw_listen(port);
        if (lst) break;

        port++;
        tryCount--;
    }
    fprintf(stderr, "createServer socket: %p\n", lst);
    if (!lst)
    {
        fprintf(stderr, "No available ports...\n");
        dw->destroy();
        return NULL;
    }

    //----------------
    HTTPServer *server = (HTTPServer *) malloc( sizeof(HTTPServer) );
    fprintf(stderr, "createServer server: %p\n", server);
    if (!server)
    {
        sw_close(lst);
        dw->destroy();
        return NULL;
    }

    server->lst = lst;
    server->port = port;
    server->dw = dw;

    pthread_create(&server->thread, 0, (void *(*)(void*))acceptSocket, server);

    return server;
}

//-------------------------------------------------------------------------
int getServerPort(HTTPServer *server)
{
    if (!server) return -1;
    return server->port;
}

//-------------------------------------------------------------------------
int64 getServerBytesRead(HTTPServer *server)
{
    if (!server) return 0;
    return server->dw->bytesRead();
}

//-------------------------------------------------------------------------
int64 getServerBadSectorCount(HTTPServer *server)
{
    if (!server) return 0;
    return server->dw->badSectorCount();
}

//-------------------------------------------------------------------------
void destroyServer(HTTPServer *server)
{
    if (!server) return;

    if (server->lst) { sw_close(server->lst); server->lst = NULL; }
    if (server->thread) pthread_join(server->thread, NULL);
    if (server->dw) server->dw->destroy();

    free(server);
    return;
}





