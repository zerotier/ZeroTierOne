#import <Foundation/Foundation.h>

@interface Clipboard : NSObject {
    
}

- (void) copy:(NSString*)text;
- (NSString *) paste;

@end
