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

#import <Cocoa/Cocoa.h>

@class NetworkMonitor;
@class Network;
@class NodeStatus;

@interface AppDelegate : NSObject <NSApplicationDelegate>

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

- (void)closeJoinNetworkPopover;

- (void)darkModeChanged:(NSNotification*)note;

@end
