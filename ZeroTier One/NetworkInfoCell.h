//
//  NetworkInfoCell.h
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

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
@property (weak, nonatomic) IBOutlet NSButton *connectedCheckbox;
@property (weak, nonatomic) IBOutlet NSButton *deleteButton;

- (IBAction)onConnectCheckStateChanged:(NSButton*)sender;
- (IBAction)deleteNetwork:(NSButton*)sender;
- (IBAction)onAllowStatusChanged:(NSButton*)sender;

- (void)joinNetwork:(NSString*)nwid;
- (void)leaveNetwork:(NSString*)nwid;

@end
