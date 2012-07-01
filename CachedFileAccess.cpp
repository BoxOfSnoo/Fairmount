
#include "IFileAccess.h"

#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

//-------------------------------------------------------------------------
class CachedFileAccess : public IFileAccess
{
public:
    CachedFileAccess(IFileAccess *fa);
    ~CachedFileAccess();

    virtual int64 size() const;
    virtual const char * path() const;

    virtual int64 bytesRead() const;
    virtual int64 badSectorCount() const;

    virtual byte *lock(int64 start, int64 length);
    virtual void unlock();

    virtual void destroy();

    void loadNextBlock();

private:
    enum
    {
        HEADER_SIZE = 8 * 1024 * 1024,
        BLOCK_SIZE = 2 * 1024 * 1024        // must be smaller than HEADER_SIZE
    };

    IFileAccess *mFA;

    byte *mHeader;

    byte *mCurrentBlock;
    int64 mCurrentBlockStart;

    byte *mNextBlock;
    int64 mNextBlockStart;

    pthread_t mThread;

    pthread_mutex_t mMutex;
};


//-------------------------------------------------------------------------
IFileAccess * IFileAccess::createCached(IFileAccess *fa)
{
    if (!fa) return NULL;

    return new CachedFileAccess(fa);
}

//-------------------------------------------------------------------------
CachedFileAccess::CachedFileAccess(IFileAccess *fa)
:
    mFA(fa),

    mHeader(new byte[HEADER_SIZE]),

    mCurrentBlock(new byte[BLOCK_SIZE]),
    mCurrentBlockStart(0),

    mNextBlock(new byte[BLOCK_SIZE]),
    mNextBlockStart(0),

    mThread(NULL)
{
    pthread_mutex_init(&mMutex, NULL);

    int64 s = size();
    int64 load = HEADER_SIZE;
    if (load > s) load = s;

    byte *b = mFA->lock(0, load);
    if (!b)
    {
        delete [] mHeader;
        mHeader = NULL;
    }
    else
    {
        memcpy(mHeader, b, load);
        mFA->unlock();
    }
}

//-------------------------------------------------------------------------
CachedFileAccess::~CachedFileAccess()
{
    if (mThread) pthread_join(mThread, NULL);

    pthread_mutex_destroy(&mMutex);

    mFA->destroy();
    if (mHeader) delete [] mHeader;
    delete [] mCurrentBlock;
    delete [] mNextBlock;
}

//-------------------------------------------------------------------------
void CachedFileAccess::destroy()
{
    delete this;
}

//-------------------------------------------------------------------------
int64 CachedFileAccess::size() const
{
    return mFA->size();
}

//-------------------------------------------------------------------------
const char * CachedFileAccess::path() const
{
    return mFA->path();
}

//-------------------------------------------------------------------------
int64 CachedFileAccess::bytesRead() const
{
    return mFA->bytesRead();
}

//-------------------------------------------------------------------------
int64 CachedFileAccess::badSectorCount() const
{
    return mFA->badSectorCount();
}

//-------------------------------------------------------------------------
static void * loadNextBlock(void* arg)
{
    CachedFileAccess *cfa = (CachedFileAccess*)arg;
    cfa->loadNextBlock();
    return NULL;
}

//-------------------------------------------------------------------------
void CachedFileAccess::loadNextBlock()
{
    int64 s = size();
    assert(mNextBlockStart < s);

    int64 load = BLOCK_SIZE;
    if (mNextBlockStart + load > s) load = s - mNextBlockStart;

    byte *b = mFA->lock(mNextBlockStart, load);
    if (!b)
    {
        mNextBlockStart = 0;
//      printf("NEXT BLOCK LOADING ERROR\n");
        return;
    }

    memcpy(mNextBlock, b, load);
    mFA->unlock();

//  printf("NEXT BLOCK LOADED\n");
}

//-------------------------------------------------------------------------
byte * CachedFileAccess::lock(int64 start, int64 length)
{
    int64 s = size();
    if (start + length > s) return NULL;

//  printf("\tLOCK %lld-%lld (%lld-%lld)\n", start, start + length, mCurrentBlockStart, mCurrentBlockStart + BLOCK_SIZE);
    pthread_mutex_lock(&mMutex);

    if (mHeader && start + length <= HEADER_SIZE)
    {
//      printf("CACHED HEADER\n");
        return mHeader + start;
    }

    if (start >= mCurrentBlockStart && start + length <= mCurrentBlockStart + BLOCK_SIZE)
    {
//      printf("CACHED BLOCK\n");
        return mCurrentBlock + (start - mCurrentBlockStart);
    }

    if (mThread)
    {
        pthread_join(mThread, NULL);
        mThread = NULL;
    }

    if (start >= mNextBlockStart && start + length <= mNextBlockStart + BLOCK_SIZE)
    {
//      printf("CACHED BLOCK - loading next\n");

        byte *b = mCurrentBlock;
        mCurrentBlock = mNextBlock;
        mCurrentBlockStart = mNextBlockStart;
        mNextBlock = b;
    }
    else
    {
//      printf("CACHE MISS - loading immediately\n");

        mCurrentBlockStart = start;
        if (mCurrentBlockStart + BLOCK_SIZE > s)
            mCurrentBlockStart = s - BLOCK_SIZE;

        int64 load = BLOCK_SIZE;
        if (mCurrentBlockStart + load > s)
            load = s - mCurrentBlockStart;

        byte *b = mFA->lock(mCurrentBlockStart, load);
        if (!b)
        {
            pthread_mutex_unlock(&mMutex);
            return NULL;
        }

        memcpy(mCurrentBlock, b, load);
        mFA->unlock();
    }

    mNextBlockStart = mCurrentBlockStart + BLOCK_SIZE;
    if (mNextBlockStart >= s) mNextBlockStart = 0;
    else pthread_create(&mThread, 0, ::loadNextBlock, this);

    assert(mCurrentBlockStart <= start);
    return mCurrentBlock + (start - mCurrentBlockStart);
}

//-------------------------------------------------------------------------
void CachedFileAccess::unlock()
{
//  printf("\tUNLOCK\n");
    pthread_mutex_unlock(&mMutex);
}

