#import "Fairmount.h"
#import "Overlay.h"
#include "Decryption.h"
#include "ListFiles.h"

#ifndef DEBUG
#define DEBUG // might as well always leave it on...
#endif

#ifdef DEBUG
    #define LOG(args...) NSLog(args) // debugging log
#else
    #define LOG(args...)
#endif

#define STATUS(args...) do { LOG(args); [self setStatus:[NSString stringWithFormat:args]]; } while(0)

#define INSTALL_PATH [@"~/Library/Application Support/Fairmount/libdvdcss.dylib" stringByExpandingTildeInPath]

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
static void sigpipeHangler(int i) { }
static void sigtermHangler(int i) { [NSApp terminate:nil]; }

//-------------------------------------------------------------------------
int main(int argc, const char **argv)
{
    signal(SIGPIPE, sigpipeHangler);
    signal(SIGTERM, sigtermHangler);
    setenv("DVDCSS_CACHE", "off", 1);
    setenv("DVDCSS_VERBOSE", "1", 1);
    return NSApplicationMain(argc, argv);
}

//-------------------------------------------------------------------------
static void DADiskAppearedCb(DADiskRef disk, void *context)
{
    [(Fairmount*)context diskAppearedCb:disk];
}

//-------------------------------------------------------------------------
static void DADiskDisappearedCb(DADiskRef disk, void *context)
{
    [(Fairmount*)context diskDisappearedCb:disk];
}

//-------------------------------------------------------------------------
static void DACallback(DADiskRef disk, DADissenterRef dissenter, void * context)
{
    int err = 0;
    if (dissenter)
    {
        err = DADissenterGetStatus(dissenter);
        if (err)
            LOG(@"err: %i (%@)\n", err, DADissenterGetStatusString(dissenter));
    }
    [(DVDServer*)context requestDone:err];
}

//-------------------------------------------------------------------------
static NSString * VolumeName(DADiskRef disk)
{
    if (!disk) return @"?";

    const char *bsdName = DADiskGetBSDName(disk);
    if (!bsdName) bsdName = "?";

    NSString *name = [NSString stringWithUTF8String:bsdName];

    NSDictionary *dsc = (NSDictionary *)DADiskCopyDescription(disk);
    if (!dsc) return name;

    NSString *volName = [dsc objectForKey:(NSString*)kDADiskDescriptionVolumeNameKey];
    if (volName)
        name = [[volName retain] autorelease];

    [dsc release];
    return name;
}

//-------------------------------------------------------------------------
static NSString * MountPoint(DADiskRef disk)
{
    if (!disk) return nil;

    NSDictionary *dsc = (NSDictionary *)DADiskCopyDescription(disk);
    if (!dsc) return nil;

    NSURL *mountPoint = [dsc objectForKey:(NSString*)kDADiskDescriptionVolumePathKey];
    if (mountPoint)
        mountPoint = [[mountPoint retain] autorelease];

    [dsc release];
    return (mountPoint) ? [mountPoint path] : nil;
}

//-------------------------------------------------------------------------
static BOOL ContainsVideoTSFolder(NSString *path)
{
    path = [path stringByAppendingPathComponent:@"VIDEO_TS"];

    NSFileManager *fm = [NSFileManager defaultManager];
    BOOL isDir;
    BOOL exists = [fm fileExistsAtPath:path isDirectory:&isDir];

    return isDir && exists;
}

//-------------------------------------------------------------------------
static BOOL ShouldTakeOver(DADiskRef disk)
{
    if (!disk) return NO;

    NSString *mountPoint = MountPoint(disk);
    if (!mountPoint) return NO;

    if (!ContainsVideoTSFolder(mountPoint)) return NO;

    NSDictionary *dsc = (NSDictionary *)DADiskCopyDescription(disk);
    if (!dsc) return NO;

    BOOL shouldTakeOver = NO;

    NSString *mediaType = [dsc objectForKey:(NSString*)kDADiskDescriptionMediaTypeKey];
    if (mediaType && [mediaType rangeOfString:@"DVD-ROM"].location != NSNotFound)
        shouldTakeOver = YES;

    [dsc release];
    return shouldTakeOver;
}

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
@implementation DVDServer
- (id) initWithSession:(DASessionRef)session tableView:(NSTableView*)tableView
{
    if ((self = [super init]))
    {
        if (!session)
        {
            [self release];
            return nil;
        }

        mDASession = (DASessionRef)CFRetain(session);
        mTableView = [tableView retain];
    }
    return self;
}

//-------------------------------------------------------------------------
- (DADiskRef) decryptedDisk
{
    return mDecryptedDisk;
}

//-------------------------------------------------------------------------
- (NSString *) volumeName
{
    if (mOriginalDisk) return VolumeName(mOriginalDisk);
    if (mDecryptedDisk) return VolumeName(mDecryptedDisk);
    return @"?";
}

//-------------------------------------------------------------------------
- (NSString *) status
{
    if (mStatus) return mStatus;
    return @"";
}

//-------------------------------------------------------------------------
- (void) setStatus:(NSString*)status
{
    if (mStatus) [mStatus release];
    if (!status) status = @"";
    mStatus = [status copy];

    [mTableView reloadData];
    [mTableView displayIfNeeded];
}

//-------------------------------------------------------------------------
- (int64) bytesRead
{
    if (mHTTPServer) return getServerBytesRead(mHTTPServer);
    return 0;
}

//-------------------------------------------------------------------------
- (int64) badSectorCount
{
    if (mHTTPServer) return getServerBadSectorCount(mHTTPServer);
    return 0;
}

//-------------------------------------------------------------------------
/*
    A small word on mRequestingService and mCallbackCalled:
    DA functions sometimes calls the callback immediately
    and sometimes it needs a trip with the run loop.
    In order to make sure we always get the results immediatly
    and don't start/stop run loops for incorrectly, we use these BOOLs.
*/
#define PRE_REQUEST mRequestingService = YES; mCallbackCalled = NO
#define POST_REQUEST mRequestingService = NO; while (!mCallbackCalled) CFRunLoopRun()
- (void) requestDone:(int)errCode
{
    mCallbackCalled = YES;
    mErrCode = errCode;
    if (!mRequestingService) CFRunLoopStop(CFRunLoopGetCurrent());
}

//-------------------------------------------------------------------------
- (void) takeOverDisk:(DADiskRef)disk
{
    Overlay *overlay = [[Overlay alloc] init];

    mOriginalDisk = (DADiskRef)CFRetain(disk);

    // get file list
    STATUS(@"Listing files...");
    NSString *mountPoint = MountPoint(disk);
    std::vector<FMFileInfo> fileList = ListFiles([[mountPoint stringByAppendingPathComponent:@"VIDEO_TS"] UTF8String]);

    // unmount original
    STATUS(@"Unmounting original disk...");
    if (overlay) [overlay setStep:1];

    PRE_REQUEST;
    DADiskUnmount(  disk, kDADiskUnmountOptionForce,
                    DACallback, self);
    POST_REQUEST;
    if (mErrCode) { LOG(@"DADiskUnmount error."); goto error; }

    // start web server
    STATUS(@"Starting data server...");
    if (overlay) [overlay setStep:2];

    NSString *bsd;
    NSString *device;
    bsd = [NSString stringWithUTF8String:DADiskGetBSDName(mOriginalDisk)];
    device = [@"/dev" stringByAppendingPathComponent:bsd];
    mHTTPServer = createServer([device UTF8String], fileList);
    if (!mHTTPServer) { LOG(@"Error creating http server."); goto error; }

    // mount remote image
    STATUS(@"Mounting image...");
    if (overlay) [overlay setStep:3];

    NSTask *mountTask;
    mountTask = [[[NSTask alloc] init] autorelease];
    if (!mountTask) { LOG(@"Error creating a NSTask."); goto error; }

    NSPipe *pipe;
    pipe = [NSPipe pipe];
    if (!pipe) { LOG(@"Error creating a NSPipe."); goto error; }

    NSString *url;
    url = [NSString stringWithFormat:@"http://127.0.0.1:%i/dvd.iso", getServerPort(mHTTPServer)];

    [mountTask setLaunchPath:@"/usr/bin/hdiutil"];
    [mountTask setArguments:[NSArray arrayWithObjects:@"attach", @"-plist", url, nil]];
    [mountTask setStandardOutput:pipe];

    [mountTask launch];

    // read back plist
    NSFileHandle *fh;
    NSMutableData *dataOutput;
    int fd;
    fh = [pipe fileHandleForReading];
    dataOutput = [[[NSMutableData alloc] init] autorelease];
    fd = [fh fileDescriptor];
    while(1)
    {
        byte buffer[1024];
        int ret = read(fd, buffer, 1024);
        if (ret <= 0) break;

        [dataOutput appendBytes:buffer length:ret];
    }

    [mountTask waitUntilExit];
    int ret;
    ret = [mountTask terminationStatus];
    if (ret != 0) { LOG(@"mountTask terminationStatus error."); goto error; }
    if (!dataOutput || [dataOutput length] == 0) { LOG(@"mountTask data return size error."); goto error; }

    // find decrypted disk
    NSDictionary *dict;
    dict = [NSPropertyListSerialization propertyListFromData:dataOutput
                                            mutabilityOption:NSPropertyListImmutable
                                                        format:nil
                                            errorDescription:nil];
    if (!dict) { LOG(@"NSPropertyListSerialization error."); goto error; }

    NSArray *entities;
    entities = [dict objectForKey:@"system-entities"];
    if (!entities || [entities count] == 0) { LOG(@"system-entities error."); goto error; }

    NSDictionary *entity;
    NSString *decryptedDevice;
    entity = [entities objectAtIndex:0];
    decryptedDevice = [entity objectForKey:@"dev-entry"];
    if (!decryptedDevice) { LOG(@"dev-entry error."); goto error; }

    mDecryptedDisk = DADiskCreateFromBSDName(nil, mDASession, [decryptedDevice UTF8String]);
    if (!mDecryptedDisk) { LOG(@"DADiskCreateFromBSDName error."); goto error; }

    // everything is ok!
    STATUS(@"Fair disk mounted.");
    if (overlay)
    {
        [overlay setStep:4];
        [overlay release];
    }
    return;

error:
    LOG(@"Bailing out on error...\n");
    STATUS(@"Error while taking over disk...");
    if (overlay) [overlay release];
    [self stopServingAndEject:NO];
}

//-------------------------------------------------------------------------
- (BOOL) stopServingAndEject:(BOOL)eject
{
    DADiskRef decryptedDisk = mDecryptedDisk; mDecryptedDisk = nil;
    if (decryptedDisk)
    {
        NSString *mountPoint = MountPoint(decryptedDisk);
        if (mountPoint)
        {
            STATUS(@"Unmounting the disk...");
            PRE_REQUEST;
            DADiskUnmount(  decryptedDisk, kDADiskUnmountOptionForce,
                            DACallback, self);
            POST_REQUEST;
            if (mErrCode)
            {
                LOG(@"Unmounting error %i...\n", mErrCode);
                mDecryptedDisk = decryptedDisk;
                STATUS(@"Disk in use...");
                sleep(1);
                STATUS(@"Fair disk mounted.");
                return NO;
            }
        }

        PRE_REQUEST;
        DADiskEject(    decryptedDisk, kDADiskEjectOptionDefault,
                        DACallback, self);
        POST_REQUEST;
        if (mErrCode)
            LOG(@"Ejecting error %i...\n", mErrCode);

        CFRelease(decryptedDisk);
        decryptedDisk = nil;
    }

    if (mHTTPServer)
    {
        STATUS(@"Shutting down data server...");

        destroyServer(mHTTPServer);
        mHTTPServer = nil;
    }

    if (mOriginalDisk)
    {
        PRE_REQUEST;
        if (eject)
        {
            STATUS(@"Ejecting original disk...");
            DADiskEject(mOriginalDisk, kDADiskEjectOptionDefault,
                        DACallback, self);
        }
        else
        {
            STATUS(@"Mounting original disk...");
            DADiskMount(mOriginalDisk, nil, kDADiskMountOptionDefault,
                        DACallback,  self);
        }
        POST_REQUEST;
        if (mErrCode)
            LOG(@"Mounting/Ejecting original error %i...\n", mErrCode);

        CFRelease(mOriginalDisk);
        mOriginalDisk = nil;
    }

    STATUS(@"All done!");
    return YES;
}

//-------------------------------------------------------------------------
- (void) dealloc
{
    if (mOriginalDisk) CFRelease(mOriginalDisk);
    if (mDecryptedDisk) CFRelease(mDecryptedDisk);
    if (mDASession) CFRelease(mDASession);
    if (mHTTPServer) destroyServer(mHTTPServer);
    if (mStatus) [mStatus release];
    if (mTableView) [mTableView release];
    [super dealloc];
}
@end

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
@implementation Fairmount
- (id) init
{
    if ((self = [super init]))
    {
        mDASession = DASessionCreate(nil);
        if (!mDASession)
        {
            [self release];
            return nil;
        }

        mServers = [[NSMutableArray alloc] init];
        if (!mServers)
        {
            [self release];
            return nil;
        }
    }
    return self;
}

//-------------------------------------------------------------------------
- (void) dealloc
{
    if (mServers) [mServers release];
    if (mDASession) CFRelease(mDASession);

    [super dealloc];
}

//-------------------------------------------------------------------------

- (void) downloadVLC:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://download.videolan.org/libdvdcss/last/macosx/libdvdcss.pkg"]];
}

- (void) retryVLC:(id)sender { [NSApp stopModal]; }

//-------------------------------------------------------------------------
- (void) awakeFromNib
{
    NSArray *libPaths = [NSArray arrayWithObjects:
                             [[NSBundle mainBundle] pathForResource:@"libdvdcss.2" ofType:@"dylib"],
                             INSTALL_PATH,
                             [@"~/Library/Application Support/Fairmount/libdvdcss.2.dylib" stringByExpandingTildeInPath],
                             @"/usr/lib/libdvdcss.2.dylib",
                             nil];
    BOOL found;

    while (1) {
        found = NO;

        for (NSString *path in libPaths) {
            if (InitDecryption(path)) {
                NSLog(@"found dvdcss in %@", path);
                found = YES;
                break;
            }
        }

        if (found) break;

        // tell user to install and retry
        [NSApp runModalForWindow:mVLCWindow];
        [mVLCWindow orderOut:nil];

    }

    DASessionScheduleWithRunLoop(mDASession, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    DARegisterDiskAppearedCallback(mDASession, nil,
                                   DADiskAppearedCb, self);

    DARegisterDiskDisappearedCallback(mDASession, nil,
                                      DADiskDisappearedCb, self);

    [self autoReloadTableView];
}

//-------------------------------------------------------------------------
- (void) autoReloadTableView
{
    [mTableView reloadData];
    [self performSelector:@selector(autoReloadTableView) withObject:nil afterDelay: 2 /* seconds */ ];
}

//-------------------------------------------------------------------------
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    while([mServers count] > 0)
    {
        DVDServer *server = [mServers objectAtIndex:0];
        BOOL ok = [server stopServingAndEject:NO];
        if (!ok)
            return NSTerminateCancel;

        [mServers removeObjectAtIndex:0];
        [mTableView reloadData];
    }

    DAUnregisterCallback(mDASession, (void*)DADiskAppearedCb, self);
    DAUnregisterCallback(mDASession, (void*)DADiskDisappearedCb, self);
    return NSTerminateNow;
}

//-------------------------------------------------------------------------
- (void) diskAppearedCb:(DADiskRef)disk
{
    LOG(@"(%s) appeared!\n", DADiskGetBSDName(disk));

    if (ShouldTakeOver(disk))
        [self takeOver:disk];
}

//-------------------------------------------------------------------------
- (void) diskDisappearedCb:(DADiskRef)disk
{
    const char *diskName = DADiskGetBSDName(disk);
    if (!diskName)
        return;

    LOG(@"(%s) disappeared!\n", DADiskGetBSDName(disk));

    int c = [mServers count], i;
    for (i = 0; i < c; i++)
    {
        DVDServer *server = [mServers objectAtIndex:i];
        DADiskRef decryptedDisk = [server decryptedDisk];
        if (!decryptedDisk) continue;

        const char *myDiskName = DADiskGetBSDName(decryptedDisk);
        if (!myDiskName)
        {
            // something is going wrong...
            [server stopServingAndEject:NO];
            return;
        }

        if (strcmp(diskName, myDiskName) == 0)
        {
            [server stopServingAndEject:YES];
            [mServers removeObjectAtIndex:i];
            [mTableView reloadData];
            break;
        }
    }
}

//-------------------------------------------------------------------------
- (void) takeOver:(DADiskRef)disk
{
    DVDServer *server = [[DVDServer alloc] initWithSession:mDASession tableView:mTableView];

    [mServers addObject:server];
    [mTableView reloadData];

    [server takeOverDisk:disk];

    if (![server decryptedDisk])
    {
        [mServers removeLastObject];
        [mTableView reloadData];
    }

    [server release];
}

//-------------------------------------------------------------------------
// NSTableDataSource protocol
- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [mServers count];
}

//-------------------------------------------------------------------------
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
    if (rowIndex < 0 || rowIndex >= [mServers count]) return nil;

    DVDServer *server = [mServers objectAtIndex:rowIndex];
    NSString *identifier = [aTableColumn identifier];

    if ([identifier isEqual:@"dvd"])
        return [server volumeName];

    else if ([identifier isEqual:@"status"])
        return [server status];

    else if ([identifier isEqual:@"bad"])
        return [NSString stringWithFormat:@"%lld", [server badSectorCount]];

    else if ([identifier isEqual:@"data"])
    {
        int64 amount = [server bytesRead];
        const int kilobyte = 1024;
        const int megabyte = 1024*1024;
        const int gigabyte = 1024*1024*1024;

        if (amount < kilobyte) return [NSString stringWithFormat:@"%lld B", amount];
        if (amount < megabyte) return [NSString stringWithFormat:@"%.2f KB", amount / (double)kilobyte];
        if (amount < gigabyte) return [NSString stringWithFormat:@"%.2f MB", amount / (double)megabyte];
        return [NSString stringWithFormat:@"%.2f GB", amount / (double)gigabyte];
    }

    return nil;
}

//-------------------------------------------------------------------------
- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject forTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
    if (rowIndex < 0 || rowIndex >= [mServers count]) return;
    DVDServer *server = [mServers objectAtIndex:rowIndex];
    NSString *identifier = [aTableColumn identifier];

    if ([identifier isEqual:@"eject"])
    {
        BOOL ok = [server stopServingAndEject:YES];
        if (ok) [mServers removeObjectAtIndex:rowIndex];
    }
    else if ([identifier isEqual:@"icon"])
    {
        [[NSWorkspace sharedWorkspace] openFile:[@"/Volumes/" stringByAppendingPathComponent:[server volumeName]]];
    }
    [mTableView reloadData];
}

//-------------------------------------------------------------------------
- (NSString *)tableView:(NSTableView *)tv toolTipForCell:(NSCell *)cell rect:(NSRectPointer)rect
    tableColumn:(NSTableColumn *)tc row:(int)row mouseLocation:(NSPoint)mouseLocation
{
    NSString *identifier = [tc identifier];
    if      ([identifier isEqual:@"icon"])      return @"Opens the disc in the Finder.";
    else if ([identifier isEqual:@"dvd"])       return @"Disc volume name.";
    else if ([identifier isEqual:@"status"])    return @"Current status.";
    else if ([identifier isEqual:@"data"])      return @"Amount of data read from disc.";
    else if ([identifier isEqual:@"bad"])       return @"Number of bad sector found on disc.";
    else if ([identifier isEqual:@"eject"])     return @"Ejects the disc.";
    return nil;
}

@end

