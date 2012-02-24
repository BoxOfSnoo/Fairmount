
#import "Overlay.h"

#include <sys/time.h>

//--------------------
#define IMG_SIZE (256)

//----------------------------------------------------------------------------------
@implementation Overlay
- (id) init
{
	NSRect frame = NSMakeRect(0, 0, 640, 480);
	NSArray *screens = [NSScreen screens];
	if ([screens count] > 0)
	{
		NSScreen *scr = [screens objectAtIndex:0];
		frame = [scr frame];
	}
	
	NSRect content = NSMakeRect((frame.size.width - IMG_SIZE) / 2,
								(frame.size.height - IMG_SIZE) / 4,
								IMG_SIZE, IMG_SIZE);
	
	if ((self = [super initWithContentRect:content
						styleMask:NSBorderlessWindowMask
						backing:NSBackingStoreBuffered
						defer:NO]))
	{
		mImageView = [[NSImageView alloc] initWithFrame:NSMakeRect(0, 0, IMG_SIZE, IMG_SIZE)];
		if (!mImageView)
		{
			[self release];
			return nil;
		}
		
		[mImageView setImageFrameStyle:NSImageFrameNone];
		
		[self setHasShadow:NO];
		[self setCanHide:NO];
		[self setExcludedFromWindowsMenu:YES];
		[self setLevel:NSScreenSaverWindowLevel];
		[self setContentView:mImageView];
		[self setBackgroundColor:[NSColor clearColor]];
		[self setAlphaValue:0];
		[self setOpaque:NO];

		[self orderFrontRegardless];
	}
	return self;
}

//----------------------------------------------------------------------------------
- (void) setStep:(int)step
{
	NSString *imagePath = [NSString stringWithFormat:@"fmprog%i.png", step];
	imagePath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:imagePath];
	
	NSImage *image = [[NSImage alloc] initWithContentsOfFile:imagePath];
	
	[mImageView setImage:image];
	[mImageView displayIfNeeded];
	
	if (image) [image release];
	else NSLog(@"Image not found: %@", imagePath);
	
	#define FADE_TIME (0.3)
	if (step == 1 || step == 4)
	{
		struct timeval start;
		gettimeofday(&start, nil);
		while(1)
		{
			struct timeval cur;
			gettimeofday(&cur, nil);
		
			struct timeval diff;
			diff.tv_sec = cur.tv_sec - start.tv_sec;
			diff.tv_usec = cur.tv_usec - start.tv_usec;
			
			double elapsed = diff.tv_sec + diff.tv_usec / 1000000. ;
			if (elapsed > FADE_TIME) break;
			
			double alpha = elapsed / FADE_TIME;
			if (step == 4) alpha = 1 - alpha;
			
			[self setAlphaValue:alpha];
			[mImageView displayIfNeeded];
		}
		
		[self setAlphaValue:(step == 1) ? 1 : 0];
	}
}

//----------------------------------------------------------------------------------
- (void) dealloc
{
	if (mImageView) [mImageView release];
	[super dealloc];
}

//----------------------------------------------------------------------------------
- (BOOL) ignoresMouseEvents
{
	return YES;
}

//----------------------------------------------------------------------------------
- (BOOL) canBecomeKeyWindow
{
	return NO;
}

@end

