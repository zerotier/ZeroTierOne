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


extern NSString * const JoinedNetworksKey;

@class AppDelegate;

@interface JoinNetworkViewController : NSViewController <NSComboBoxDelegate, NSComboBoxDataSource>

@property (nonatomic, weak) IBOutlet NSComboBox *network;
@property (nonatomic, weak) IBOutlet NSButton *joinButton;
@property (nonatomic, weak) IBOutlet NSButton *allowManagedCheckBox;
@property (nonatomic, weak) IBOutlet NSButton *allowGlobalCheckBox;
@property (nonatomic, weak) IBOutlet NSButton *allowDefaultCheckBox;
@property (nonatomic, weak) IBOutlet AppDelegate *appDelegate;

@property (nonatomic) NSMutableArray<NSString*> *values;

- (IBAction)onJoinClicked:(id)sender;


@end
