#import "Path.h"

@implementation Path

@synthesize application;
@synthesize resource;
@synthesize documents;
@synthesize library;
@synthesize home;
@synthesize temp;

- (NSString *)application {
    return [[NSBundle mainBundle] bundlePath];
}

- (NSString *)resource {
    return [[NSBundle mainBundle] resourcePath];
}

- (NSString *)documents {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    return [paths objectAtIndex:0];
}

- (NSString *)library {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
    NSLog( @"%@", paths );
    return [paths objectAtIndex:0];
}

- (NSString *)home {
    return NSHomeDirectory();
}

- (NSString *)temp {
    return NSTemporaryDirectory();
}

#pragma mark WebScripting Protocol

/* checks whether a selector is acceptable to be called from JavaScript */
+ (BOOL) isSelectorExcludedFromWebScript:(SEL)selector
{
    return NO;
}

// right now exclude all properties (eg keys)
+ (BOOL) isKeyExcludedFromWebScript:(const char*)name
{
	return NO;
}

@end
