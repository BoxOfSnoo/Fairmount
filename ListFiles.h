

#ifndef LISTFILES_H
#define LISTFILES_H

#include "Types.h"
#include <vector>
#include <string>

typedef struct
{
	std::string name;
	int64 start;
	int64 size;
} FMFileInfo;

std::vector<FMFileInfo> ListFiles(const char *videoTS);

#endif /* LISTFILES_H */
