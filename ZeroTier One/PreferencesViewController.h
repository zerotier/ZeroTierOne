//
//  PreferencesViewController.h
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface PreferencesViewController : NSViewController

@property (nonatomic, weak) IBOutlet NSButton *startupCheckBox;

- (IBAction)onStartupCheckBoxChanged:(NSButton*)sender;

- (BOOL)isLaunchAtStartup;
- (LSSharedFileListItemRef)itemRefInLoginItems;
- (void)setLaunchAtLoginEnabled:(BOOL)enabled;

@end
