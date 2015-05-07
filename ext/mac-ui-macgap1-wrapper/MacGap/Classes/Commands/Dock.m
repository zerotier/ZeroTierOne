#import "Dock.h"

@implementation Dock

@synthesize badge;

- (void) setBadge:(NSString *)value
{
    NSDockTile *tile = [[NSApplication sharedApplication] dockTile];
    [tile setBadgeLabel:value];
}

- (NSString *) badge
{
    NSDockTile *tile = [[NSApplication sharedApplication] dockTile];
    return [tile badgeLabel];
}

#pragma mark WebScripting Protocol

+ (BOOL) isSelectorExcludedFromWebScript:(SEL)selector
{
    return NO;
}

+ (BOOL) isKeyExcludedFromWebScript:(const char*)name
{
	return NO;
}

@end
