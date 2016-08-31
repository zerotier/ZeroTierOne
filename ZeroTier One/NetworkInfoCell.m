//
//  NetworkInfoCell.m
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

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
                                       error:&error];

    if (error) {
        // TODO: Display error message
    }
}

- (void)leaveNetwork:(NSString*)nwid
{
    NSError *error = nil;
    [[ServiceCom sharedInstance] leaveNetwork:nwid error:&error];

    if (error) {
        // TODO: Display error message
    }
}

@end
