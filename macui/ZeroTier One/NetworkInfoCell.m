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

#import "NetworkInfoCell.h"
#import "ServiceCom.h"
#import "ShowNetworksViewController.h"
#import "Network.h"

@implementation NetworkInfoCell

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    
    // Drawing code here.
}

- (IBAction)onConnectCheckStateChanged:(NSButton*)sender
{
    if(sender.state == NSOnState) {
        [self joinNetwork:self.networkIdField.stringValue];
    }
    else {
        [self leaveNetwork:self.networkIdField.stringValue];
    }
}

- (IBAction)deleteNetwork:(NSButton*)sender;
{
    [self leaveNetwork:self.networkIdField.stringValue];
    [self.parent deleteNetworkFromList:self.networkIdField.stringValue];
}

- (IBAction)onAllowStatusChanged:(NSButton*)sender
{
    [self joinNetwork:self.networkIdField.stringValue];
}

- (void)joinNetwork:(NSString*)nwid
{
    NSError *error = nil;
    [[ServiceCom sharedInstance] joinNetwork:nwid
                                allowManaged:(self.allowManaged.state == NSOnState)
                                 allowGlobal:(self.allowGlobal.state  == NSOnState)
                                allowDefault:![Network defaultRouteExists:_parent.networkList] && (self.allowDefault.state == NSOnState)
                                    allowDNS:(self.allowDNS.state == NSOnState)
                                       error:&error];

    if (error) {
        NSAlert *alert = [NSAlert alertWithError:error];
        alert.alertStyle = NSCriticalAlertStyle;
        [alert addButtonWithTitle:@"Ok"];

        [alert runModal];
    }
}

- (void)leaveNetwork:(NSString*)nwid
{
    NSError *error = nil;
    [[ServiceCom sharedInstance] leaveNetwork:nwid error:&error];

    if (error) {
        NSAlert *alert = [NSAlert alertWithError:error];
        alert.alertStyle = NSCriticalAlertStyle;
        [alert addButtonWithTitle:@"Ok"];

        [alert runModal];
    }
}

@end
