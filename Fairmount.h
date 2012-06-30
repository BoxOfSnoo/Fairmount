
#import <Cocoa/Cocoa.h>
#import <DiskArbitration/DiskArbitration.h>

#include "Types.h"
#include "HTTPServer.h"

//----------------------------------------------------------------------------------
@interface DVDServer : NSObject
{
	DASessionRef	mDASession;
	DADiskRef		mOriginalDisk;
	DADiskRef		mDecryptedDisk;
	HTTPServer		*mHTTPServer;
	BOOL			mRequestingService, mCallbackCalled;
	NSString		*mStatus;
	NSTableView		*mTableView;
	int				mErrCode;
}
- (id) initWithSession:(DASessionRef)session tableView:(NSTableView*)tableView;

- (void) requestDone:(int)errCode;
- (DADiskRef) decryptedDisk;
- (NSString *) volumeName;
- (NSString *) status;
- (void) setStatus:(NSString*)status;
- (int64) bytesRead;
- (int64) badSectorCount;

- (void) takeOverDisk:(DADiskRef)disk;

// returns true on success
- (BOOL) stopServingAndEject:(BOOL)eject;
@end

//----------------------------------------------------------------------------------
@interface Fairmount : NSObject
{
	IBOutlet NSTableView	*mTableView;

	NSMutableArray			*mServers;
	DASessionRef			mDASession;
	
	IBOutlet NSWindow		*mVLCWindow;
}

- (void) downloadVLC:(id)sender;
- (void) retryVLC:(id)sender;

- (void) diskAppearedCb:(DADiskRef)disk;
- (void) diskDisappearedCb:(DADiskRef)disk;

- (void) takeOver:(DADiskRef)disk;

- (void) autoReloadTableView;
@end
