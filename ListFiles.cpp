
#include "ListFiles.h"

#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

//-------------------------------------------------------------------------
static int noCurAndParDir(struct dirent *entry)
{
    if ((strncmp(entry->d_name, ".", 256) == 0) || (strncmp(entry->d_name, "..", 256) == 0))
        return 0;
    else
        return 1;
}

//-------------------------------------------------------------------------
std::vector<FMFileInfo> ListFiles(const char *videoTS)
{
    std::vector<FMFileInfo> files;
    if (!videoTS) return files;

    std::string folder = videoTS;
    int fln = folder.size();
    if (fln == 0) return files;
    if (folder[fln - 1] != '/') folder += '/';

    std::vector<std::string> filePaths;

    struct dirent **nameList = NULL;
    int numOfEntries = scandir(folder.c_str(), &nameList, noCurAndParDir, alphasort);
    if (numOfEntries == -1) return files;

    for (int i = 0; i < numOfEntries; i++)
    {
        std::string path = nameList[i]->d_name;
        filePaths.push_back(path);
        free(nameList[i]);
    }
    free(nameList);

    for (int i = 0; i < filePaths.size(); i++)
    {
        std::string fullPath = folder + filePaths[i];
        const char *cpath = fullPath.c_str();

        int fd = open(cpath, O_RDONLY, 0);
        if (fd == -1) continue;

        struct log2phys physicalPosition;
        int ret = fcntl(fd, F_LOG2PHYS, (void*)(&physicalPosition));
        close(fd);

        if (ret == -1) continue;

        struct stat st;
        if (stat(cpath, &st) != 0) continue;
        if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)) continue;

        FMFileInfo info;
        info.name = filePaths[i];
        info.start = physicalPosition.l2p_devoffset;
        info.size = st.st_size;

        files.push_back(info);

//      printf("name: %s start: %lld size: %lld\n",
//              info.name.c_str(), info.start, info.size);
    }

    return files;
}


