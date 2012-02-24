


#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include "Types.h"
#include "ListFiles.h"

typedef struct _HTTPServer HTTPServer;

HTTPServer* createServer(const char *device, std::vector<FMFileInfo> fileList);
int getServerPort(HTTPServer *server);
int64 getServerBytesRead(HTTPServer *server);
int64 getServerBadSectorCount(HTTPServer *server);
void destroyServer(HTTPServer *server);

#endif /* HTTPSERVER_H */

