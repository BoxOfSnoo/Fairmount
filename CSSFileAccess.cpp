
#include "IFileAccess.h"

#include "Decryption.h"
#include "FileSize.h"
#include "ListFiles.h"

#include <string>
#include <vector>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

//-------------------------------------------------------------------------
typedef struct
{
    // in blocks, inclusive
    int start;
    int end;
} VOBBoundary;

//-------------------------------------------------------------------------
class CSSFileAccess : public IFileAccess
{
public:
    CSSFileAccess(const char *path, int64 size, dvdcss_t dvd, std::vector<VOBBoundary> vobBounds);
    ~CSSFileAccess();

    virtual int64 size() const;
    virtual const char * path() const;

    virtual int64 bytesRead() const;
    virtual int64 badSectorCount() const;

    virtual byte *lock(int64 start, int64 length);
    virtual void unlock();

    virtual void destroy();

private:
    dvdcss_t mDVD;

    int64 mSize;
    std::string mPath;
    std::vector<VOBBoundary> mVOBBoundaries;

    byte *mData;
    pthread_mutex_t mMutex;
    int64 mBytesRead;
    int64 mBadSectorCount;
};

//-------------------------------------------------------------------------
static std::vector<VOBBoundary> findVobBounds(const std::vector<FMFileInfo> &fileList)
{
    std::vector<VOBBoundary> vobBounds;

    int c = fileList.size();
    for (int i = 0; i < c; i++)
    {
        if (fileList[i].name.size() >= 12)
        {
            const char *s = fileList[i].name.c_str();
            if (    (s[9] == 'V')
                &&  (s[10] == 'O')
                &&  (s[11] == 'B') )
            {
                VOBBoundary vb;
                vb.start = fileList[i].start / DVDCSS_BLOCK_SIZE;
                vb.end = vb.start + (fileList[i].size / DVDCSS_BLOCK_SIZE) - 1;
                vobBounds.push_back(vb);
            }
        }
    }

#if 0
    for (int i = 0; i < vobBounds.size(); i++)
        fprintf(stderr, "bound %i start: %i end: %i length: %i\n",
                i, vobBounds[i].start, vobBounds[i].end, vobBounds[i].end - vobBounds[i].start + 1);
#endif

    return vobBounds;
}


//-------------------------------------------------------------------------
typedef struct
{
    // in blocks
    int start;
    int length;
    bool decrypt;
} SliceToRead;

//-------------------------------------------------------------------------
static std::vector<SliceToRead> sliceRequest(int start, int length, std::vector<VOBBoundary> vobBounds)
{
    std::vector<SliceToRead> slices;

    int end = start + length - 1;
    int c = vobBounds.size(), i;

    while(length > 0)
    {
        SliceToRead sr;
        sr.start = start;

        // check if start is inside a vob
        bool insideVob = false;
        for (i = 0; i < c; i++)
        {
            if (start >= vobBounds[i].start && start <= vobBounds[i].end)
            {
                sr.decrypt = true;

                if (end <= vobBounds[i].end)
                    sr.length = length;
                else
                    sr.length = vobBounds[i].end - start + 1;

                slices.push_back(sr);

                insideVob = true;
                start += sr.length;
                length -= sr.length;
                break;
            }
        }
        if (insideVob) continue;

        // we have a normal file
        sr.decrypt = false;

        // find nearest vob
        int nearestStart = -1;
        for (i = 0; i < c; i++)
        {
            if (vobBounds[i].start > start && (nearestStart == -1 || vobBounds[i].start < nearestStart))
                nearestStart = vobBounds[i].start;
        }

        if (nearestStart == -1)
            // we are after all vobs
            sr.length = end - start + 1;
        else
        {
            sr.length = nearestStart - start;
            if (sr.length > length) sr.length = length;
        }

        slices.push_back(sr);

        start += sr.length;
        length -= sr.length;
    }

    return slices;
}

//-------------------------------------------------------------------------
IFileAccess * IFileAccess::createCSS(const char *path, std::vector<FMFileInfo> fileList)
{
    if (!path) return NULL;

    int64 size = FileSize(path, NULL, NULL);
    fprintf(stderr, "createCSS FileSize: %lld\n", size);
    if (size == 0) return NULL;

    dvdcss_t dvd = dvdcss_open((char*)path);
    fprintf(stderr, "createCSS dvd: %p\n", dvd);
    if (!dvd) return NULL;

    CSSFileAccess *cfa = new CSSFileAccess(path, size, dvd, findVobBounds(fileList));
    return cfa;
}

//-------------------------------------------------------------------------
CSSFileAccess::CSSFileAccess(const char *path, int64 size, dvdcss_t dvd, std::vector<VOBBoundary> vobBounds)
:
    mDVD(dvd),
    mSize(size),
    mPath(path),
    mVOBBoundaries(vobBounds),
    mData(NULL),
    mBytesRead(0),
    mBadSectorCount(0)
{
    pthread_mutex_init(&mMutex, NULL);
}

//-------------------------------------------------------------------------
CSSFileAccess::~CSSFileAccess()
{
    pthread_mutex_destroy(&mMutex);
    dvdcss_close(mDVD);
    if (mData) delete [] mData;
}

//-------------------------------------------------------------------------
void CSSFileAccess::destroy()
{
    delete this;
}

//-------------------------------------------------------------------------
int64 CSSFileAccess::size() const
{
    return mSize;
}

//-------------------------------------------------------------------------
const char * CSSFileAccess::path() const
{
    return mPath.c_str();
}

//-------------------------------------------------------------------------
int64 CSSFileAccess::bytesRead() const
{
    return mBytesRead;
}

//-------------------------------------------------------------------------
int64 CSSFileAccess::badSectorCount() const
{
    return mBadSectorCount;
}

//-------------------------------------------------------------------------
byte * CSSFileAccess::lock(int64 start, int64 length)
{
    if (start + length > mSize) return NULL;

    int64 end = start + length - 1;
    int64 blockStart = start / DVDCSS_BLOCK_SIZE;
    int64 blockEnd = end / DVDCSS_BLOCK_SIZE;
    int64 blockLength =  blockEnd - blockStart + 1;
    int64 offset = start - blockStart * DVDCSS_BLOCK_SIZE;

    int newLength = blockLength * DVDCSS_BLOCK_SIZE;

    pthread_mutex_lock(&mMutex);

    if (mData) delete [] mData;
    mData = new byte[newLength];
    assert(mData);

    std::vector<SliceToRead> slices = sliceRequest(blockStart, blockLength, mVOBBoundaries);
    int c = slices.size(), i;

#if 0
    printf("sliceRequest %lld %lld\n", blockStart, blockLength);
    for (i = 0; i < c; i++)
        printf("\tslice %i: decrypt %i doNotRead %i start %i length %i (%i)\n",
                    i, slices[i].decrypt, slices[i].doNotRead, slices[i].start, slices[i].length,
                    slices[i].start + slices[i].length);
#endif

    byte *data = mData;
    for (i = 0; i < c; i++)
    {
        int start = slices[i].start;
        int length = slices[i].length;
        bool decrypt = slices[i].decrypt;
        bool damagedDisc = false;

        int ret;
        int readDecryptFlags = decrypt ? DVDCSS_READ_DECRYPT : DVDCSS_NOFLAGS;
        int seekDecryptFlags = decrypt ? (DVDCSS_SEEK_KEY | DVDCSS_SEEK_MPEG) : DVDCSS_NOFLAGS;

        ret = dvdcss_seek(mDVD, start, seekDecryptFlags);
        if (ret == start)
        {
            ret = dvdcss_read(mDVD, data, length, readDecryptFlags);
            if (ret == length)
                data += length * DVDCSS_BLOCK_SIZE;
            else
                damagedDisc = true;
        }
        else
            damagedDisc = true;

        if (damagedDisc)
        {
//          fprintf(stderr, "damagedDisc (start %i length %i)\n", start, length);

            // There was a read error, either because the disc is damaged
            // or because of arccos. Revert to reading sector by sector...

            while(length > 0)
            {
                bool badSector = false;

                ret = dvdcss_seek(mDVD, start, seekDecryptFlags);
                if (ret == start)
                {
                    ret = dvdcss_read(mDVD, data, 1, readDecryptFlags);
                    if (ret == 1)
                    {
//                      fprintf(stderr, "sector ok at %i\n", start);
                        start++;
                        length--;
                        data += DVDCSS_BLOCK_SIZE;
                    }
                    else
                        badSector = true;
                }
                else
                    badSector = true;

                if (badSector)
                {
                    // According to how ECC is done on DVDs,
                    // bad sectors always come in pack of 16 sectors.

                    int next16Start = (start + 16) & 0xFFFFFFF0;
                    int skipLength = next16Start - start;
                    if (skipLength > length) skipLength = length;

//                  fprintf(stderr, "sectors unreadable at %i count %i\n", start, skipLength);

                    memset(data, 0, skipLength * DVDCSS_BLOCK_SIZE);

                    mBadSectorCount += skipLength;
                    start += skipLength;
                    length -= skipLength;
                    data += skipLength * DVDCSS_BLOCK_SIZE;
                }
            }
        }
    }

    mBytesRead += newLength;
    return mData + offset;
}

//-------------------------------------------------------------------------
void CSSFileAccess::unlock()
{
    pthread_mutex_unlock(&mMutex);
}

