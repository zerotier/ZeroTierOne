/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
    self.webView.policyDelegate = self;
    
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
