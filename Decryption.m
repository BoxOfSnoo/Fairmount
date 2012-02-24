

#include "Decryption.h"
#include <dlfcn.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <architecture/byte_order.h>

#import <Cocoa/Cocoa.h>

/*
	Fairmount doesn't include any decryption functions by itself.
	It will use a decryption library installed by the user, if available.
*/

//----------------------------------------------------------------------------------
dvdcss_t (*dvdcss_open)(char *) = 0;
int (*dvdcss_close)(dvdcss_t) = 0;
int (*dvdcss_seek)(dvdcss_t, int, int) = 0;
int (*dvdcss_read)(dvdcss_t, void *, int, int) = 0;

//----------------------------------------------------------------------------------
static int LoadLib(const char *path)
{
	void *dvdcss_library = dlopen(path, RTLD_NOW);
	if (!dvdcss_library)
	{
		const char *error = dlerror();
		if (error) printf("dlerror: %s\n", error);
		return -1;
	}
    dvdcss_open = (dvdcss_t (*)(char*))					dlsym(dvdcss_library, "dvdcss_open");
    dvdcss_close = (int (*)(dvdcss_t))					dlsym(dvdcss_library, "dvdcss_close");
    dvdcss_seek = (int (*)(dvdcss_t, int, int))			dlsym(dvdcss_library, "dvdcss_seek");
    dvdcss_read = (int (*)(dvdcss_t, void*, int, int))	dlsym(dvdcss_library, "dvdcss_read");

	if(!dvdcss_open || !dvdcss_close || !dvdcss_seek || !dvdcss_read)
	{
		dlclose(dvdcss_library);
		return -1;
	}
	
	fprintf(stderr, "loaded dvdcss from %s\n", path);
	return 0;	
}

//----------------------------------------------------------------------------------
static int ParseDirectory(NSString *path, NSFileManager *fm, NSString **libpath)
{
	BOOL isDir;
	BOOL exists = [fm fileExistsAtPath:path isDirectory:&isDir];
	
	if (!exists) return -1;
	if (!isDir)
	{
		if ([path rangeOfString:@"dvdcss"].location != NSNotFound)
		{
			const char *fs = [path fileSystemRepresentation];
			int ret = LoadLib(fs);
			if (ret == 0)
			{
				if (libpath) *libpath = path;
				return 0;
			}
		}
		else
			return -1;
	}

	NSArray *a = [fm subpathsAtPath:path];
	int c = [a count], i;
	for (i = 0; i < c; i++)
	{
		NSString *cur = [path stringByAppendingPathComponent:[a objectAtIndex:i]];
		exists = [fm fileExistsAtPath:cur isDirectory:&isDir];
		if (!exists) continue;
		
		if (isDir)
		{
			int ret = ParseDirectory(cur, fm, libpath);
			if (ret == 0) return 0;
		}
		else if ([cur rangeOfString:@"dvdcss"].location != NSNotFound)
		{
			const char *fs = [cur fileSystemRepresentation];
			int ret = LoadLib(fs);
			if (ret == 0)
			{
				if (libpath) *libpath = cur;
				return 0;
			}
		}
	}
	
	return -1;
}

//----------------------------------------------------------------------------------
int InitDecryption(NSString *tryPath, NSString **libpath)
{
	if (libpath) *libpath = nil;
	
	NSFileManager *fm = [NSFileManager defaultManager];
	if (!fm) return -1;
	
	return ParseDirectory(tryPath, fm, libpath);
}
