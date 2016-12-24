#import "ContentView.h"
#import "WebViewDelegate.h"
#import "AppDelegate.h"
#import "JSEventHelper.h"

@interface WebPreferences (WebPreferencesPrivate)
    - (void)_setLocalStorageDatabasePath:(NSString *)path;
    - (void) setLocalStorageEnabled: (BOOL) localStorageEnabled;
    - (void) setDatabasesEnabled:(BOOL)databasesEnabled;
    - (void) setDeveloperExtrasEnabled:(BOOL)developerExtrasEnabled;
    - (void) setWebGLEnabled:(BOOL)webGLEnabled;
    - (void) setOfflineWebApplicationCacheEnabled:(BOOL)offlineWebApplicationCacheEnabled;
@end

@implementation ContentView

@synthesize webView, delegate, mainMenu;

- (void) awakeFromNib
{
    WebPreferences *webPrefs = [WebPreferences standardPreferences];
    
    NSString *cappBundleName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];    
    NSString *applicationSupportFile = [@"~/Library/Application Support/" stringByExpandingTildeInPath];
    NSString *savePath = [NSString pathWithComponents:[NSArray arrayWithObjects:applicationSupportFile, cappBundleName, @"LocalStorage", nil]];
    [webPrefs _setLocalStorageDatabasePath:savePath];
    [webPrefs setLocalStorageEnabled:YES];
    [webPrefs setDatabasesEnabled:YES];
    [webPrefs setDeveloperExtrasEnabled:[[NSUserDefaults standardUserDefaults] boolForKey: @"developer"]];
    [webPrefs setOfflineWebApplicationCacheEnabled:YES];
    [webPrefs setWebGLEnabled:YES];
    
    [self.webView setPreferences:webPrefs];
    
    NSHTTPCookieStorage *cookieStorage = [NSHTTPCookieStorage 
                                          sharedHTTPCookieStorage]; 
    [cookieStorage setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];
    
    [self.webView setApplicationNameForUserAgent: @"MacGap"];
    
	self.delegate = [[WebViewDelegate alloc] initWithMenu:[NSApp mainMenu]];
//	[self.webView setFrameLoadDelegate:self.delegate];
//	[self.webView setUIDelegate:self.delegate];
//	[self.webView setResourceLoadDelegate:self.delegate];
//	[self.webView setDownloadDelegate:self.delegate];
//	[self.webView setPolicyDelegate:self.delegate];
    [self.webView setDrawsBackground:NO];
    [self.webView setShouldCloseWithWindow:NO];
    
    [self.webView setGroupName:@"MacGap"];

}

- (void) windowResized:(NSNotification*)notification;
{
	NSWindow* window = (NSWindow*)notification.object;
	NSSize size = [window frame].size;
	
	DebugNSLog(@"window width = %f, window height = %f", size.width, size.height);
    
    bool isFullScreen = (window.styleMask & NSFullScreenWindowMask) == NSFullScreenWindowMask;
    int titleBarHeight = isFullScreen ? 0 : [[Utils sharedInstance] titleBarHeight:window];
    
	[self.webView setFrame:NSMakeRect(0, 0, size.width, size.height - titleBarHeight)];
    [JSEventHelper triggerEvent:@"orientationchange" forWebView:self.webView];
}

@end
