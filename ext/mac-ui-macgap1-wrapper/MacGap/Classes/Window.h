#import <Foundation/Foundation.h>

#import "WindowController.h"

@interface Window : NSObject{
    CGRect _oldRestoreFrame;
}

@property (retain, nonatomic) WindowController *windowController;
@property (nonatomic, retain) WebView *webView;

- (id) initWithWebView:(WebView *)view;
- (void) open:(NSDictionary *)properties;
- (void) move:(NSDictionary *)properties;
- (void) resize:(NSDictionary *) properties;
- (Boolean) isMaximized;
- (CGFloat) getX;
- (CGFloat) getY;
- (void) maximize;
- (void) restore;
- (void) toggleFullscreen;

@end
