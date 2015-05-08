//
//  UserDefaults.h
//  MacGap
//
//  Created by Jeff Hanbury on 16/04/2014.
//  Copyright (c) 2014 Twitter. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "WindowController.h"

@interface UserDefaults : NSObject

@property (nonatomic, retain) WebView *webView;

- (id) initWithWebView:(WebView *)view;
- (NSString*) getMyDefaults;
- (NSDictionary*) myDefaultsDictionary;
- (void) removeObjectForKey:(NSString*)key;
- (NSArray*) getUserDefaultsKeys;

- (NSString*) addPrefix:(NSString*)key;

- (void) setString:(NSString*)key withValue:(NSString*)value;
- (NSString*) getString:(NSString*)key;

- (void) setInteger:(NSString*)key withValue:(NSString*)value;
- (NSNumber*) getInteger:(NSString*)key;

- (void) setBool:(NSString*)key withValue:(NSString*)value;
- (NSNumber*) getBool:(NSString*)key;

- (void) setFloat:(NSString*)key withValue:(NSString*)value;
- (NSNumber*) getFloat:(NSString*)key;

// Could also be implemented:
//– setObject:forKey:
//– setDouble:forKey:
//– setURL:forKey:

@end

