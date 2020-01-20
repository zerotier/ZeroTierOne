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

#import "ShowNetworksViewController.h"
#import "NetworkMonitor.h"
#import "NetworkInfoCell.h"
#import "Network.h"

BOOL hasNetworkWithID(NSArray<Network*> *list, UInt64 nwid)
{
    for(Network *n in list) {
        if(n.nwid == nwid) {
            return YES;
        }
    }

    return NO;
}

@interface ShowNetworksViewController ()

@end

@implementation ShowNetworksViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.networkList = [NSMutableArray array];

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

- (NSInteger)findNetworkWithID:(UInt64)networkId
{
    for(int i = 0; i < [_networkList count]; ++i) {
        Network *nw = [_networkList objectAtIndex:i];
        
        if(nw.nwid == networkId) {
            return i;
        }
    }
    
    return NSNotFound;
}


- (void)deleteNetworkFromList:(NSString *)nwid {
    [self.netMonitor deleteSavedNetwork:nwid];
    
    UInt64 netid = 0;
    NSScanner *scanner = [NSScanner scannerWithString:nwid];
    [scanner scanHexLongLong:&netid];
    for (Network *n in _networkList) {
        if (n.nwid == netid) {
            NSInteger index = [self findNetworkWithID:netid];
            
            if (index != NSNotFound) {
                [_networkList removeObjectAtIndex:index];
                [_tableView reloadData];
            }
        }
    }
}

- (void)setNetworks:(NSArray<Network *> *)list {
    for (Network *n in list) {
        if ([_networkList containsObject:n]) {
            // don't need to do anything here.  Already an identical object in the list
            continue;
        }
        else {
            // network not in the list based on equality.  Did an object change? or is it a new item?
            if (hasNetworkWithID(_networkList, n.nwid)) {

                for (int i = 0; i < [_networkList count]; ++i) {
                    Network *n2 = [_networkList objectAtIndex:i];
                    if (n.nwid == n2.nwid)
                    {
                        [_networkList replaceObjectAtIndex:i withObject:n];
                        [_tableView reloadDataForRowIndexes:[NSIndexSet indexSetWithIndex:i]
                                              columnIndexes:[NSIndexSet indexSetWithIndex:0]];
                    }
                }
            }
            else {
                [_networkList addObject:n];
                [_tableView reloadData];
            }
        }
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
