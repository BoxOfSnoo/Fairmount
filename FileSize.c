
#include "FileSize.h"

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/disk.h>
#include <string.h>
#include <unistd.h>

//-------------------------------------------------------------------------
int64 FileSize(const char *path, int *blockSize, int64 *blockCount)
{
    struct stat st;

    if (blockSize) *blockSize = 0;
    if (blockCount) *blockCount = 0;

    //  if we can't stat, assume it doesn't exist
    if (stat(path, &st) != 0)
        return 0;

    if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode))
    {
        if (blockSize) *blockSize = 1;
        if (blockCount) *blockCount = st.st_size;
        return st.st_size;
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd == -1)
    {
        fprintf(stderr, "Can't open %s: %s (%d)\n", path, strerror(errno), errno);
        return 0;
    }

    int block_size;
    int err = ioctl(fd, DKIOCGETBLOCKSIZE, &block_size);
    if (err)
    {
        fprintf(stderr, "Can't get block size of %s: %s (%d)\n", path, strerror(errno), errno);
        close(fd);
        return 0;
    }

    int64 block_count;
    err = ioctl(fd, DKIOCGETBLOCKCOUNT, &block_count);
    if (err)
    {
        fprintf(stderr, "Can't get block count of %s: %s (%d)\n", path, strerror(errno), errno);
        close(fd);
        return 0;
    }

    close(fd);

    if (blockSize) *blockSize = block_size;
    if (blockCount) *blockCount = block_count;

    return (int64)block_size * block_count;
}

