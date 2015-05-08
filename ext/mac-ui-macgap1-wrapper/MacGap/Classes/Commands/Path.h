#import <Foundation/Foundation.h>

@interface Path : NSObject {
    
}

- (NSString *) application;
- (NSString *) resource;
- (NSString *) documents;
- (NSString *) library;
- (NSString *) home;
- (NSString *) temp;

@property (readonly,copy) NSString* application;
@property (readonly,copy) NSString* resource;
@property (readonly,copy) NSString* documents;
@property (readonly,copy) NSString* library;
@property (readonly,copy) NSString* home;
@property (readonly,copy) NSString* temp;

@end
