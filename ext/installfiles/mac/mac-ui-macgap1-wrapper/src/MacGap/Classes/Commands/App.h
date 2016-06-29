#import <Foundation/Foundation.h>

#import "WindowController.h"

@interface App : NSObject {

}

@property (nonatomic, retain) WebView *webView;

- (id) initWithWebView:(WebView *)view;

- (void) terminate;
- (void) activate;
- (void) hide;
- (void) unhide;
- (void) beep;
- (void) bounce;
- (void) setCustomUserAgent:(NSString *)userAgentString;
- (NSNumber*) systemIdleTime;
@end
