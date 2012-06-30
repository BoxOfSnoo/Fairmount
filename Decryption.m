

#include <dlfcn.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <architecture/byte_order.h>

#import <Cocoa/Cocoa.h>

#include "Decryption.h"

/*
    Fairmount doesn't include any decryption functions by itself.
    It will use a decryption library installed by the user, if available.
*/

//-------------------------------------------------------------------------
dvdcss_t (*dvdcss_open)(char *) = 0;
int (*dvdcss_close)(dvdcss_t) = 0;
int (*dvdcss_seek)(dvdcss_t, int, int) = 0;
int (*dvdcss_read)(dvdcss_t, void *, int, int) = 0;

//-------------------------------------------------------------------------
static int LoadLib(const char *path)
{
    void *dvdcss_library = dlopen(path, RTLD_NOW);
    if (!dvdcss_library)
    {
        const char *error = dlerror();
        if (error) fprintf(stderr, "dlerror: %s\n", error);
        return -1;
    }
    dvdcss_open = (dvdcss_t (*)(char*))                 dlsym(dvdcss_library, "dvdcss_open");
    dvdcss_close = (int (*)(dvdcss_t))                  dlsym(dvdcss_library, "dvdcss_close");
    dvdcss_seek = (int (*)(dvdcss_t, int, int))         dlsym(dvdcss_library, "dvdcss_seek");
    dvdcss_read = (int (*)(dvdcss_t, void*, int, int))  dlsym(dvdcss_library, "dvdcss_read");

    if(!dvdcss_open || !dvdcss_close || !dvdcss_seek || !dvdcss_read)
    {
        dlclose(dvdcss_library);
        return -1;
    }

    fprintf(stderr, "loaded dvdcss from %s\n", path);
    return 0;
}

//-------------------------------------------------------------------------
BOOL InitDecryption(NSString *path)
{
    NSFileManager *fm;
    BOOL isDir, exists;

    fm = [NSFileManager defaultManager];
    if (!fm) return NO;

    exists = [fm fileExistsAtPath:path isDirectory:&isDir];

    if (!exists || isDir) return NO;

    if ([path rangeOfString:@"dvdcss"].location == NSNotFound)
      return NO;

    if (LoadLib([path fileSystemRepresentation]) == 0)
      return YES;
    else
      return NO;
}
