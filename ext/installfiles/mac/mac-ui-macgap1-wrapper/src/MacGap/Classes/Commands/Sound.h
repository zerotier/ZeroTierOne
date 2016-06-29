#import <Cocoa/Cocoa.h>
#import "Command.h"
#import "CallbackDelegate.h"


@interface Sound : Command {

}

// pending callbacks for sounds being played, to keep
// ARC from freeing them too early
@property (nonatomic, strong) NSMutableSet *pending;

- (void) play:(NSString*)file onComplete:(WebScriptObject*)callback;
- (void) playSystem:(NSString*)name onComplete:(WebScriptObject*)callback;

@end
