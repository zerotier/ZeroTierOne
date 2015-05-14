#import "WindowController.h"


@interface WindowController() {

}

-(void) notificationCenter;

@end

@implementation WindowController

@synthesize  contentView, url;

- (id) initWithURL:(NSString *) relativeURL{
    self = [super initWithWindowNibName:@"Window"];
    self.url = [NSURL URLWithString:relativeURL relativeToURL:[[NSBundle mainBundle] resourceURL]];
    
    [self.window setFrameAutosaveName:@"MacGapWindow"];
    [self notificationCenter];

    return self;
}

-(id) initWithRequest: (NSURLRequest *)request{
    self = [super initWithWindowNibName:@"Window"];
    [self notificationCenter];
    [[self.contentView.webView mainFrame] loadRequest:request];
    
    return self;
}

-(void) notificationCenter{
    [[NSNotificationCenter defaultCenter] addObserver:self.contentView 
                                             selector:@selector(windowResized:) 
                                                 name:NSWindowDidResizeNotification 
                                               object:[self window]];   
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
    if (self.url != nil) {
        [self.contentView.webView setMainFrameURL:[self.url absoluteString]];
    }
    
    
    // Implement this method to handle any initialization after your 
    // window controller's window has been loaded from its nib file.
}

@end
