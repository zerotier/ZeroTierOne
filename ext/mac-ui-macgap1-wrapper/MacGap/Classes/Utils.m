#import "Utils.h"
#import <Webkit/WebScriptObject.h>

static Utils* sharedInstance = nil;

@implementation Utils

- (float) titleBarHeight:(NSWindow*)aWindow
{
    NSRect frame = [aWindow frame];
    NSRect contentRect = [NSWindow contentRectForFrameRect: frame
												 styleMask: NSTitledWindowMask];
	
    return (frame.size.height - contentRect.size.height);
}

- (NSString*) pathForResource:(NSString*)resourcepath
{
    NSBundle * mainBundle = [NSBundle mainBundle];
    NSMutableArray *directoryParts = [NSMutableArray arrayWithArray:[resourcepath componentsSeparatedByString:@"/"]];
    NSString       *filename       = [directoryParts lastObject];
    [directoryParts removeLastObject];
	
    NSString *directoryStr = [NSString stringWithFormat:@"%@/%@", kStartFolder, [directoryParts componentsJoinedByString:@"/"]];
    return [mainBundle pathForResource:filename
								ofType:@""
						   inDirectory:directoryStr];
}

- (NSString*) convertDictionaryToJSON:(NSDictionary*)dict {
    // Convert defaults Dictionary to JSON.
    NSError *error;
    NSData *jsonData = [NSJSONSerialization
                        dataWithJSONObject:dict
                        options:NSJSONWritingPrettyPrinted // Pass 0 if you don't care about the readability of the generated string
                        error:&error];
    
    NSString *jsonString;
    if (! jsonData) {
        NSLog(@"Got an error converting to JSON: %@", error);
    }
    else {
        jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    }
    
    return jsonString;
}

// Convert JavaScript array (arrives as a WebScriptObject) into an NSArray of strings.
- (NSArray*) convertJSarrayToNSArray:(WebScriptObject*)jsArray {
    NSInteger count = [[jsArray valueForKey:@"length"] integerValue];
    
    NSMutableArray *args = [NSMutableArray array];
    for (int i = 0; i < count; i++) {
        NSString *item = [jsArray webScriptValueAtIndex:i];
        if ([item isKindOfClass:[NSString class]]) {
            [args addObject:item];
        }
    }
    
    return args;
}

#pragma mark -
#pragma mark Singleton methods

+ (Utils*) sharedInstance
{
    @synchronized(self)
    {
        if (sharedInstance == nil){
			sharedInstance = [[Utils alloc] init];
		 }
    }
    return sharedInstance;
}

+ (id) allocWithZone:(NSZone *)zone {
    @synchronized(self) {
        if (sharedInstance == nil) {
            sharedInstance = [super allocWithZone:zone];
            return sharedInstance;  // assignment and return on first allocation
        }
    }
    return nil; // on subsequent allocation attempts return nil
}

- (id) copyWithZone:(NSZone *)zone
{
    return self;
}

@end