//
//  JoinNetworkViewController.h
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>


extern NSString * const JoinedNetworksKey;

@interface JoinNetworkViewController : NSViewController <NSComboBoxDelegate, NSComboBoxDataSource>

@property (nonatomic, weak) IBOutlet NSComboBox *network;
@property (nonatomic, weak) IBOutlet NSButton *joinButton;
@property (nonatomic, weak) IBOutlet NSButton *allowManagedCheckBox;
@property (nonatomic, weak) IBOutlet NSButton *allowGlobalCheckBox;
@property (nonatomic, weak) IBOutlet NSButton *allowDefaultCheckBox;

@property (nonatomic) NSMutableArray<NSString*> *values;

- (IBAction)onJoinClicked:(id)sender;


@end
