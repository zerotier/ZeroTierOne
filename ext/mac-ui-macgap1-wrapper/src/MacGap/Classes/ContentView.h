#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@class WebViewDelegate;

@interface ContentView : NSView {
	IBOutlet WebView* webView;
	WebViewDelegate* delegate;
}

@property (retain) WebView* webView;
@property (retain) WebViewDelegate* delegate;
@property (strong) IBOutlet NSMenu *mainMenu;

@end
