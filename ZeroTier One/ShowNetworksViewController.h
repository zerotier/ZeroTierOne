//
//  ShowNetworksViewController.h
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class NetworkMonitor;
@class Network;

@interface ShowNetworksViewController : NSViewController<NSTableViewDelegate, NSTableViewDataSource>

@property (nonatomic) NSArray<Network*> *networkList;
@property (nonatomic) NetworkMonitor *netMonitor;
@property (nonatomic) BOOL visible;

@property (weak, nonatomic) IBOutlet NSTableView *tableView;

- (void)deleteNetworkFromList:(NSString*)nwid;
- (void)setNetworks:(NSArray<Network*>*)list;


@end
