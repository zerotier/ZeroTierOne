//
//  Notice.h
//  MacGap
//
//  Created by Christian Sullivan on 7/26/12.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "WindowController.h"

#define APP_NOTICE_NOTIFICATION @"Notice"

@interface Notice : NSObject <NSUserNotificationCenterDelegate> {
    
}

@property (nonatomic, retain) WebView *webView;

- (id) initWithWebView:(WebView *)view;
- (void) notify:(NSDictionary*)message;
- (void) close:(NSString*)notificationId;
+ (BOOL) available;

@end

