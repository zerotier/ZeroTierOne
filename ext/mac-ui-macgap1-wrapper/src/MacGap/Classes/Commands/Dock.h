#import <Foundation/Foundation.h>

@interface Dock : NSObject {
    
}
- (void) setBadge:(NSString*)value;
- (NSString *) badge;

@property (readwrite, copy) NSString *badge;

@end
