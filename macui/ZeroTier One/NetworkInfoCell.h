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

@class ShowNetworksViewController;

@interface NetworkInfoCell : NSTableCellView

@property (weak, nonatomic) ShowNetworksViewController *parent;

@property (weak, nonatomic) IBOutlet NSTextField *networkIdField;
@property (weak, nonatomic) IBOutlet NSTextField *networkNameField;
@property (weak, nonatomic) IBOutlet NSTextField *statusField;
@property (weak, nonatomic) IBOutlet NSTextField *typeField;
@property (weak, nonatomic) IBOutlet NSTextField *macField;
@property (weak, nonatomic) IBOutlet NSTextField *mtuField;
@property (weak, nonatomic) IBOutlet NSTextField *broadcastField;
@property (weak, nonatomic) IBOutlet NSTextField *bridgingField;
@property (weak, nonatomic) IBOutlet NSTextField *deviceField;
@property (weak, nonatomic) IBOutlet NSTextField *addressesField;
@property (weak, nonatomic) IBOutlet NSButton *allowManaged;
@property (weak, nonatomic) IBOutlet NSButton *allowGlobal;
@property (weak, nonatomic) IBOutlet NSButton *allowDefault;
@property (weak, nonatomic) IBOutlet NSButton *allowDNS;
@property (weak, nonatomic) IBOutlet NSButton *connectedCheckbox;
@property (weak, nonatomic) IBOutlet NSButton *deleteButton;

- (IBAction)onConnectCheckStateChanged:(NSButton*)sender;
- (IBAction)deleteNetwork:(NSButton*)sender;
- (IBAction)onAllowStatusChanged:(NSButton*)sender;

- (void)joinNetwork:(NSString*)nwid;
- (void)leaveNetwork:(NSString*)nwid;

@end
