//
//  AppDelegate.h
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class NetworkMonitor;
@class Network;
@class NodeStatus;

@interface AppDelegate : NSObject <NSApplicationDelegate, NSMenuDelegate>

@property (weak, nonatomic) IBOutlet NSWindow *window;

@property (nonatomic) NSStatusItem *statusItem;

@property (nonatomic) NSPopover *networkListPopover;
@property (nonatomic) NSPopover *joinNetworkPopover;
@property (nonatomic) NSPopover *preferencesPopover;
@property (nonatomic) NSPopover *aboutPopover;

@property (nonatomic) id transientMonitor;

@property (nonatomic) NetworkMonitor *monitor;

@property (nonatomic) NSMutableArray<Network*> *networks;

@property (nonatomic) NodeStatus *status;

- (void)buildMenu;

- (void)onNetworkListUpdated:(NSNotification*)note;
- (void)onNodeStatusUpdated:(NSNotification*)note;

- (void)showNetworks;
- (void)joinNetwork;
- (void)showPreferences;
- (void)showAbout;
- (void)quit;
- (void)toggleNetwork:(NSMenuItem*)sender;
- (void)copyNodeID;
- (void)menuWillOpen:(NSMenu*)menu;
- (void)menuDidClose:(NSMenu*)menu;

- (void)darkModeChanged:(NSNotification*)note;

@end
