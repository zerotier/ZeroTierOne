//
//  Notice.m
//  MacGap
//
//  Created by Christian Sullivan on 7/26/12.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import "Notice.h"

#import "JSEventHelper.h"

@implementation Notice

- (id) initWithWebView:(WebView*)view
{
    if(self = [super init]) {
        self.webView = view;
	    [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:self];
    }
    return self;
}

- (void) notify:(NSDictionary *)message {
    NSUserNotification *notification = [[NSUserNotification alloc] init];
    [notification setTitle:[message valueForKey:@"title"]];
    [notification setInformativeText:[message valueForKey:@"content"]];
    [notification setDeliveryDate:[NSDate dateWithTimeInterval:0 sinceDate:[NSDate date]]];
    BOOL playSound = true; // optional parameter, false only when {sound: false}
    @try {
        NSNumber *s = [message valueForKey:@"sound"];
        if ([[s className] isEqual: @"__NSCFBoolean"]) {
            playSound = [s boolValue];
        }
    }
    @catch (NSException *exception) {
    }
    if (playSound) {
        [notification setSoundName:NSUserNotificationDefaultSoundName];
    }
    NSString *id = @""; // optional, needed for close
    @try {
        id = [message valueForKey:@"id"];
    }
    @catch (NSException *exception) {
    }
    [notification setUserInfo:[NSDictionary dictionaryWithObjectsAndKeys:id, @"id", nil]];
    NSUserNotificationCenter *center = [NSUserNotificationCenter defaultUserNotificationCenter];
    [center scheduleNotification:notification];
}

// close all notifications with id == notificationId or close all notifications if notificationId == "*"
- (void) close:(NSString*)notificationId {
    NSUserNotificationCenter *center = [NSUserNotificationCenter defaultUserNotificationCenter];
    for(NSUserNotification * deliveredNote in center.deliveredNotifications) {
        if ([notificationId isEqualToString:@"*"] || [deliveredNote.userInfo[@"id"] isEqualToString:notificationId]) {
            [center removeDeliveredNotification: deliveredNote];
        }
    }
}

+ (BOOL) available {
    if ([NSUserNotificationCenter respondsToSelector:@selector(defaultUserNotificationCenter)])
        return YES;
    
    return NO;
}

- (void) userNotificationCenter:(NSUserNotificationCenter *)center didActivateNotification:(NSUserNotification *)notification
{
    NSString *notificationId = [notification.userInfo valueForKey:@"id"];
    [JSEventHelper triggerEvent:@"macgap.notify.activated" forDetail:notificationId forWebView:self.webView];
}

#pragma mark WebScripting Protocol

+ (BOOL) isSelectorExcludedFromWebScript:(SEL)selector
{
    BOOL result = YES;
    if (selector == @selector(notify:))
        result = NO;
    if (selector == @selector(close:))
        result = NO;
    
    return result;
}

+ (NSString*) webScriptNameForSelector:(SEL)selector
{
	id	result = nil;
	
	if (selector == @selector(notify:)) {
		result = @"notify";
	}
	if (selector == @selector(close:)) {
		result = @"close";
	}
	
	return result;
}

// right now exclude all properties (eg keys)
+ (BOOL) isKeyExcludedFromWebScript:(const char*)name
{
	return YES;
}

@end
