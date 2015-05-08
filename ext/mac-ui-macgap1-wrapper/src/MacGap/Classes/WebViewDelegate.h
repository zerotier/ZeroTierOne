#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@class Sound;
@class Dock;
@class Growl;
@class Notice;
@class Path;
@class App;
@class Window;
@class Clipboard;
@class Fonts;
@class MenuProxy;
@class UserDefaults;

@class WindowController;

@interface WebViewDelegate : NSObject {
	Sound* sound;
    Dock* dock;
    Growl* growl;
    Notice* notice;
    Path* path;
    App* app;
    Window* window;
    Clipboard* clipboard;
    Fonts* fonts;
    NSMenu *mainMenu;
    UserDefaults* userDefaults;
}



@property (nonatomic, retain) Sound* sound;
@property (nonatomic, retain) Dock* dock;
@property (nonatomic, retain) Growl* growl;
@property (nonatomic, retain) Notice* notice;
@property (nonatomic, retain) Path* path;
@property (nonatomic, retain) App* app;
@property (nonatomic, retain) Window* window;
@property (nonatomic, retain) Clipboard* clipboard;
@property (nonatomic, retain) Fonts* fonts;
@property (nonatomic, retain) MenuProxy* menu;
@property (nonatomic, retain) UserDefaults* userDefaults;

@property (nonatomic, retain) WindowController *requestedWindow;

- (id) initWithMenu:(NSMenu*)menu;
@end
