#import <Cocoa/Cocoa.h>
#import "ContentView.h"

@interface WindowController : NSWindowController {
    
}

- (id) initWithURL:(NSString *) url;
- (id) initWithRequest: (NSURLRequest *)request;
@property (retain) NSURL * url;
@property (retain) IBOutlet ContentView *contentView;

@end
