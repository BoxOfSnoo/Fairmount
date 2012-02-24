

#ifndef FILESIZE_H
#define FILESIZE_H

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

int64 FileSize(const char *path, int *blockSize, int64 *blockCount);

#ifdef __cplusplus
}
#endif

#endif /* FILESIZE_H  */




