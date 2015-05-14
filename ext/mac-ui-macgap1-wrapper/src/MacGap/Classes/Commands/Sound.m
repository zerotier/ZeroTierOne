#import "Sound.h"


@interface PlayDelegate : CallbackDelegate <NSSoundDelegate> {
}

@property (nonatomic, weak) Sound *sound;

- (id) initWithContext:(JSContextRef)aContext
           forCallback:(WebScriptObject*)aCallback
             withSound:(Sound*)aSound;
@end

@implementation PlayDelegate

@synthesize sound;

- (id) initWithContext:(JSContextRef)aContext
           forCallback:(WebScriptObject*)aCallback
             withSound:(Sound*)aSound
{
        self = [super initWithContext:aContext forCallback:aCallback];
        if (!self)
                return nil;
        sound = aSound;
        return self;
}

- (void)sound:(NSSound *)aSound didFinishPlaying:(BOOL)finishedPlaying {
        [self callWithParams:[aSound name], nil];
        [sound.pending removeObject:self];
}

@end

@implementation Sound

@synthesize pending;

- (id) initWithContext:(JSContextRef)aContext {
    self = [super initWithContext:aContext];
    if (!self) {
        return nil;
    }
    
    pending = [NSMutableSet new];
    return self;
}

- (void) playSound:(NSSound*)sound onComplete:(WebScriptObject*)callback {
    if (callback != (id)[WebUndefined undefined]) {
        PlayDelegate *d = [[PlayDelegate alloc] initWithContext:context
                                                    forCallback:callback
                                                      withSound:self];
        [pending addObject:d];
        [sound setDelegate:d];
    }
    [sound play];
}

- (void) play:(NSString*)file onComplete:(WebScriptObject*)callback {
	NSURL* fileUrl  = [NSURL fileURLWithPath:[[Utils sharedInstance] pathForResource:file]];
	DebugNSLog(@"Sound file:%@", [fileUrl description]);
	
	NSSound* sound = [[NSSound alloc] initWithContentsOfURL:fileUrl byReference:YES];
    [self playSound:sound onComplete:callback];
}

- (void) playSystem:(NSString*)name onComplete:(WebScriptObject*)callback {
    NSSound *systemSound = [NSSound soundNamed:name];
    [self playSound:systemSound onComplete:callback];
}

#pragma mark WebScripting Protocol

+ (BOOL) isSelectorExcludedFromWebScript:(SEL)selector {
    return [self webScriptNameForSelector:selector] == nil;
}

+ (BOOL) isKeyExcludedFromWebScript:(const char*)name {
	return YES;
}

+ (NSString*) webScriptNameForSelector:(SEL)selector {
    id	result = nil;

    if (selector == @selector(play:onComplete:)) {
            result = @"play";
    }
    else if (selector == @selector(playSystem:onComplete:)) {
            result = @"playSystem";
    }

    return result;
}

@end
