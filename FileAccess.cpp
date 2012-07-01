
#include "IFileAccess.h"

#include "FileSize.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <string>
#include <assert.h>

//-------------------------------------------------------------------------
class FileAccess : public IFileAccess
{
public:
    FileAccess(const char *path, int64 size, int fd, int blockSize);
    ~FileAccess();

    virtual int64 size() const;
    virtual const char * path() const;

    virtual int64 bytesRead() const;
    virtual int64 badSectorCount() const;

    virtual byte *lock(int64 start, int64 length);
    virtual void unlock();

    virtual void destroy();

private:
    int mFD;

    int64 mSize;
    std::string mPath;

    byte *mData;
    pthread_mutex_t mMutex;

    int mBlockSize;
    int64 mBytesRead;
};

//-------------------------------------------------------------------------
IFileAccess * IFileAccess::createFile(const char *path)
{
    if (!path) return NULL;

    int blockSize;
    int64 size = FileSize(path, &blockSize, NULL);
    if (size == 0) return NULL;

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) return NULL;

    return new FileAccess(path, size, fd, blockSize);
}

//-------------------------------------------------------------------------
FileAccess::FileAccess(const char *path, int64 size, int fd, int blockSize)
:
    mFD(fd),
    mSize(size),
    mPath(path),
    mData(NULL),
    mBlockSize(blockSize),
    mBytesRead(0)
{
    pthread_mutex_init(&mMutex, NULL);
}

//-------------------------------------------------------------------------
FileAccess::~FileAccess()
{
    pthread_mutex_destroy(&mMutex);
    close(mFD);
    if (mData) delete [] mData;
}

//-------------------------------------------------------------------------
void FileAccess::destroy()
{
    delete this;
}

//-------------------------------------------------------------------------
int64 FileAccess::size() const
{
    return mSize;
}

//-------------------------------------------------------------------------
const char * FileAccess::path() const
{
    return mPath.c_str();
}

//-------------------------------------------------------------------------
int64 FileAccess::bytesRead() const
{
    return mBytesRead;
}

//-------------------------------------------------------------------------
int64 FileAccess::badSectorCount() const
{
    return 0;
}

//-------------------------------------------------------------------------
byte * FileAccess::lock(int64 start, int64 length)
{
    if (start + length > mSize) return NULL;

    int64 end = start + length - 1;
    int64 blockStart = start / mBlockSize;
    int64 blockEnd = end / mBlockSize;
    int64 blockLength =  blockEnd - blockStart + 1;

    off_t newStart = blockStart * mBlockSize;
    ssize_t newLength = blockLength * mBlockSize;

    int64 offset = start - newStart;

    pthread_mutex_lock(&mMutex);

    if (mData) delete [] mData;
    mData = new byte[newLength];
    assert(mData);

    ssize_t size = pread(mFD, mData, newLength, newStart);
    if (size != (ssize_t) newLength)
    {
        fprintf(stderr, "Can't read %s: %s (%d)\n", mPath.c_str(), strerror(errno), errno);
        memset(mData, 0, newLength);
        return mData;
    }

    mBytesRead += newLength;
    return mData + offset;
}

//-------------------------------------------------------------------------
void FileAccess::unlock()
{
    pthread_mutex_unlock(&mMutex);
}


