//
//  Helper.h
//  MacGap
//
//  Created by Liam Kaufman Simpkins on 12-01-22.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "WindowController.h"

@interface JSEventHelper : NSObject

+ (void) triggerEvent:(NSString *)event forWebView:(WebView *)webView;
+ (void) triggerEvent:(NSString *)event withArgs:(NSDictionary *)args forWebView:(WebView *)webView;
+ (void) triggerEvent:(NSString *)event withArgs:(NSDictionary *)args forObject:(NSString *)objName forWebView:(WebView *)webView;
+ (void) triggerEvent:(NSString *)event forDetail:(NSString *)detail forWebView:(WebView *)webView;
+ (void) triggerEvent:(NSString *)event forDetail:(NSString *)detail forObject:(NSString *)objName forWebView:(WebView *)webView;

@end
