//
//  AboutViewController.m
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import "AboutViewController.h"

@interface AboutViewController ()

@end

@implementation AboutViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    [self.webView setWantsLayer:YES];
    self.webView.layer.borderWidth = 1.0f;
    [self.webView.layer setCornerRadius:1.0f];
    self.webView.layer.masksToBounds = YES;
    [self.webView.layer setBorderColor:[[NSColor darkGrayColor] CGColor]];

    NSBundle *bundle = [NSBundle mainBundle];
    NSURL *path = [bundle URLForResource:@"about" withExtension:@"html"];
    if(path) {
        [self.webView.mainFrame loadRequest:[NSURLRequest requestWithURL:path]];
    }
}

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation
        request:(NSURLRequest *)request
          frame:(WebFrame *)frame
decisionListener:(id<WebPolicyDecisionListener>)listener
{
    if(request.URL != nil && request.URL.host != nil) {
        [[NSWorkspace sharedWorkspace] openURL:request.URL];
    }
    else {
        [listener use];
    }
}

@end
