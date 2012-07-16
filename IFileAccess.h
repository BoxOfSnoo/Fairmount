

#ifndef IFILEACCESS_H
#define IFILEACCESS_H

#include "Types.h"
#include "ListFiles.h"

class IFileAccess
{
public:
    static IFileAccess * createCSS(const char *path, std::vector<FMFileInfo> fileList);
    static IFileAccess * createFile(const char *path);

    static IFileAccess * createCached(IFileAccess *fa);

    virtual int64 size() const                          = 0;
    virtual const char * path() const                   = 0;

    virtual int64 bytesRead() const                     = 0;
    virtual int64 badSectorCount() const                = 0;

    virtual byte *lock(int64 start, int64 length)       = 0;
    virtual void unlock()                               = 0;

    virtual void destroy()                              = 0;
};


#endif /* IFILEACCESS_H */
