//
//  UserDefaults.m
//  MacGap
//
//  Created by Jeff Hanbury on 16/04/2014.
//  Copyright (c) 2014 Twitter. All rights reserved.
//

#import "UserDefaults.h"
#import "JSEventHelper.h"

@interface UserDefaults() {

}

-(void) setupNotificationCenter;

@end


@implementation UserDefaults

- (id) initWithWebView:(WebView *) view{
    self = [super init];
    
    if (self) {
        self.webView = view;
        [self setupNotificationCenter];
    }
    
    return self;
}


-(void) setupNotificationCenter{
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(defaultsChanged:)
                                                 name:NSUserDefaultsDidChangeNotification
                                               object:nil];
}

- (void)defaultsChanged:(NSNotification *)notification {
    NSDictionary* returnDict = [self myDefaultsDictionary];
    [JSEventHelper triggerEvent:@"userDefaultsChanged" withArgs:returnDict forWebView:self.webView];
}

- (NSString*) getMyDefaults {
    NSDictionary* myDefaults = [self myDefaultsDictionary];

    return [[Utils sharedInstance] convertDictionaryToJSON:myDefaults];
}

- (NSDictionary*) myDefaultsDictionary {
    NSString* prefix = [kWebScriptNamespace stringByAppendingString:@"_"];
    NSMutableDictionary* returnDict = [[NSMutableDictionary alloc] init];

    // Get the user defaults.
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    
    // Build up a dictionary containing just the items beginning with our
    // prefix.
    for (NSString* key in [self getUserDefaultsKeys]) {
        if ([key hasPrefix:prefix]) {
            id val = [defaults valueForKey:key];
            [returnDict setObject:val forKey:key];
        }
    }

    return returnDict;
}

- (NSArray*) getUserDefaultsKeys {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    return [[prefs dictionaryRepresentation] allKeys];
}

- (void) removeObjectForKey:(NSString*)key {
    NSString* prefixedKey;
    prefixedKey = [self addPrefix:key];

    [[NSUserDefaults standardUserDefaults] removeObjectForKey:prefixedKey];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

// Check we have a standard prefix for JS-modified keys, for security purposes.
// If not, add it. This stops JavaScript from ever being able to modify keys
// it did not create.
- (NSString*) addPrefix:(NSString*)key {
    NSString* prefix;
    prefix = [kWebScriptNamespace stringByAppendingString:@"_"];
    
    if (![key hasPrefix:prefix]) {
        key = [prefix stringByAppendingString:key];
    }
    return key;
}

// String

- (void) setString:(NSString*)key withValue:(NSString*)value {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSString* prefixedKey;
    prefixedKey = [self addPrefix:key];
    [prefs setObject:value forKey:prefixedKey];
}

- (NSString*) getString:(NSString *)key {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    return [prefs stringForKey:key];
}

// All the following must convert their type to NSNumber for JavaScript.

// Integer

- (void) setInteger:(NSString*)key withValue:(NSString*)value {
    NSString* prefixedKey;
    prefixedKey = [self addPrefix:key];

    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSInteger myInt = [value intValue];
    [prefs setInteger:myInt forKey:prefixedKey];
}

- (NSNumber*) getInteger:(NSString *)key {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    return [NSNumber numberWithInteger:[prefs integerForKey:key]];
}

// Boolean

- (void) setBool:(NSString*)key withValue:(NSString*)value {
    NSString* prefixedKey;
    prefixedKey = [self addPrefix:key];

    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    BOOL myBool = [value boolValue];
    [prefs setBool:myBool forKey:prefixedKey];
}

- (NSNumber*) getBool:(NSString *)key {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    return [NSNumber numberWithBool:[prefs boolForKey:key]];
}

// Float

- (void) setFloat:(NSString*)key withValue:(NSString*)value {
    NSString* prefixedKey;
    prefixedKey = [self addPrefix:key];

    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    float myFloat = [value floatValue];
    [prefs setFloat:myFloat forKey:prefixedKey];
}

- (NSNumber*) getFloat:(NSString *)key {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    return [NSNumber numberWithFloat:[prefs floatForKey:key]];
}


#pragma mark WebScripting Protocol

+ (BOOL) isSelectorExcludedFromWebScript:(SEL)selector {
    return NO;
}

+ (NSString*) webScriptNameForSelector:(SEL)selector {
	id	result = nil;
	
	if (selector == @selector(getMyDefaults)) {
		result = @"getMyDefaults";
    }
    
	if (selector == @selector(removeObjectForKey:)) {
		result = @"removeObjectForKey";
    }
    
    else if (selector == @selector(setString:withValue:)) {
		result = @"setString";
    } else if (selector == @selector(getString:)) {
        result = @"getString";
    }
    
    else if (selector == @selector(setInteger:withValue:)) {
		result = @"setInteger";
    } else if (selector == @selector(getInteger:)) {
        result = @"getInteger";
    }
    
    else if (selector == @selector(setBool:withValue:)) {
		result = @"setBool";
    } else if (selector == @selector(getBool:)) {
        result = @"getBool";
    }

    else if (selector == @selector(setFloat:withValue:)) {
		result = @"setFloat";
    } else if (selector == @selector(getFloat:)) {
        result = @"getFloat";
    }

	return result;
}

+ (BOOL) isKeyExcludedFromWebScript:(const char*)name {
	return NO;
}

@end
