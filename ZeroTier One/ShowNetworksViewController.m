//
//  ShowNetworksViewController.m
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import "ShowNetworksViewController.h"
#import "NetworkMonitor.h"
#import "NetworkInfoCell.h"
#import "Network.h"

@interface ShowNetworksViewController ()

@end

@implementation ShowNetworksViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    [self.tableView setDelegate:self];
    [self.tableView setDataSource:self];
    [self.tableView setBackgroundColor:[NSColor clearColor]];
}

- (void)viewWillAppear {
    [super viewWillAppear];
    self.visible = YES;
}

- (void)viewWillDisappear {
    [super viewWillDisappear];
    self.visible = NO;
}

- (void)deleteNetworkFromList:(NSString *)nwid {
    [self.netMonitor deleteSavedNetwork:nwid];
}

- (void)setNetworks:(NSArray<Network *> *)list {
    _networkList = list;
    if(_visible) {
        [_tableView reloadData];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return [_networkList count];
}

- (NSView*)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    NetworkInfoCell *cell = (NetworkInfoCell*)[tableView makeViewWithIdentifier:@"NetworkInfoCell"
                                                                          owner:nil];
    Network *network = [_networkList objectAtIndex:row];
    cell.parent = self;
    cell.networkIdField.stringValue = [NSString stringWithFormat:@"%10llx", network.nwid];
    cell.networkNameField.stringValue = network.name;
    cell.statusField.stringValue = [network statusString];
    cell.typeField.stringValue = [network typeString];
    cell.mtuField.stringValue = [NSString stringWithFormat:@"%d", network.mtu];
    cell.macField.stringValue = network.mac;
    cell.broadcastField.stringValue = network.broadcastEnabled ? @"ENABLED" : @"DISABLED";
    cell.bridgingField.stringValue = network.bridge ? @"ENABLED" : @"DISABLED";
    cell.deviceField.stringValue = network.portDeviceName;

    if(network.connected) {
        cell.connectedCheckbox.state = NSOnState;

        if(network.allowDefault) {
            cell.allowDefault.enabled = YES;
            cell.allowDefault.state = NSOnState;
        }
        else {
            cell.allowDefault.state = NSOffState;

            if([Network defaultRouteExists:_networkList]) {
                cell.allowDefault.enabled = NO;
            }
            else {
                cell.allowDefault.enabled = YES;
            }
        }

        cell.allowGlobal.enabled = YES;
        cell.allowManaged.enabled = YES;
    }
    else {
        cell.connectedCheckbox.state = NSOffState;
        cell.allowDefault.enabled = NO;
        cell.allowGlobal.enabled = NO;
        cell.allowManaged.enabled = NO;
    }

    cell.allowGlobal.state = network.allowGlobal ? NSOnState : NSOffState;
    cell.allowManaged.state = network.allowManaged ? NSOnState : NSOffState;

    cell.addressesField.stringValue = @"";

    for(NSString *addr in network.assignedAddresses) {
        cell.addressesField.stringValue = [[cell.addressesField.stringValue stringByAppendingString:addr] stringByAppendingString:@"\n"];
    }

    return cell;
}

@end
