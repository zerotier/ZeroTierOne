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

#import "NetworkMonitor.h"
#import "Network.h"
#import "ServiceCom.h"
#import "NodeStatus.h"

@import AppKit;


NSString * const NetworkUpdateKey = @"com.zerotier.one.network-list";
NSString * const StatusUpdateKey = @"com.zerotier.one.status";

@interface NetworkMonitor (private)

- (NSString*)dataFile;
- (void)internal_updateNetworkInfo;
- (NSInteger)findNetworkWithID:(UInt64)networkId;
- (NSInteger)findSavedNetworkWithID:(UInt64)networkId;
- (void)saveNetworks;

@end

@implementation NetworkMonitor

- (id)init
{
    self = [super init];
    if(self)
    {
        _savedNetworks = [NSMutableArray<Network*> array];
        _receivedNetworks = [NSArray<Network*> array];
        _allNetworks = [NSMutableArray<Network*> array];
        _timer = nil;
    }

    return self;
}

- (void)dealloc
{
    [_timer invalidate];
}

- (void)start
{
    NSLog(@"ZeroTier monitor started");
    _timer = [NSTimer scheduledTimerWithTimeInterval:1.0f
                                              target:self
                                            selector:@selector(updateNetworkInfo)
                                            userInfo:nil
                                             repeats:YES];
}

- (void)stop
{
    NSLog(@"ZeroTier monitor stopped");
    [_timer invalidate];
    _timer = nil;
}

- (void)updateNetworkInfo
{
    NSString *filePath = [self dataFile];

    if([[NSFileManager defaultManager] fileExistsAtPath:filePath]) {
        NSArray<Network*> *networks = [NSKeyedUnarchiver unarchiveObjectWithFile:filePath];

        if(networks != nil) {
            _savedNetworks = [networks mutableCopy];
        }
    }

    NSError *error = nil;

    [[ServiceCom sharedInstance] getNetworklist:^(NSArray<Network *> *networkList) {
        _receivedNetworks = networkList;

        [[NSOperationQueue mainQueue] addOperationWithBlock:^{
            [self internal_updateNetworkInfo];
        } ];
    } error:&error];

    if(error) {
        [self stop];

        NSAlert *alert = [NSAlert alertWithError:error];
        alert.alertStyle = NSCriticalAlertStyle;
        [alert addButtonWithTitle:@"Quit"];
        [alert addButtonWithTitle:@"Retry"];

        NSModalResponse res = [alert runModal];

        if(res == NSAlertFirstButtonReturn) {
            [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
        }
        else if(res == NSAlertSecondButtonReturn) {
            [self start];
            return;
        }
    }

    [[ServiceCom sharedInstance] getNodeStatus:^(NodeStatus *status) {
        NSDictionary *userInfo = [NSDictionary dictionaryWithObject:status forKey:@"status"];

        [[NSOperationQueue mainQueue] addOperationWithBlock:^{
            [[NSNotificationCenter defaultCenter] postNotificationName:StatusUpdateKey
                                                                object:nil
                                                              userInfo:userInfo];
        }];
    } error:&error];

    if (error) {
        [self stop];

        NSAlert *alert = [NSAlert alertWithError:error];
        alert.alertStyle = NSCriticalAlertStyle;
        [alert addButtonWithTitle:@"Quit"];
        [alert addButtonWithTitle:@"Retry"];

        NSModalResponse res = [alert runModal];

        if(res == NSAlertFirstButtonReturn) {
            [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
        }
        else if(res == NSAlertSecondButtonReturn) {
            [self start];
            return;
        }
    }
}

- (void)deleteSavedNetwork:(NSString*)networkId
{
    UInt64 nwid = 0;
    NSScanner *scanner = [NSScanner scannerWithString:networkId];
    [scanner scanHexLongLong:&nwid];

    NSInteger index = [self findNetworkWithID:nwid];

    if(index != NSNotFound) {
        [_allNetworks removeObjectAtIndex:index];
    }

    index = [self findSavedNetworkWithID:nwid];

    if(index != NSNotFound) {
        [_savedNetworks removeObjectAtIndex:index];
    }

    [self saveNetworks];
}

@end

@implementation NetworkMonitor (private)
- (NSString*)dataFile
{
    NSURL *appSupport = [[[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory
                                                               inDomains:NSUserDomainMask] objectAtIndex:0];

    appSupport = [[[appSupport URLByAppendingPathComponent:@"ZeroTier"] URLByAppendingPathComponent:@"One"] URLByAppendingPathComponent:@"networkinfo.dat"];
    return appSupport.path;
}

- (void)internal_updateNetworkInfo
{
    NSMutableArray<Network*> *networks = [_savedNetworks mutableCopy];

    for(Network *nw in _receivedNetworks) {
        NSInteger index = [self findSavedNetworkWithID:nw.nwid];

        if(index != NSNotFound) {
            [networks setObject:nw atIndexedSubscript:index];
        }
        else {
            [networks addObject:nw];
        }
    }

    [networks sortUsingComparator:^NSComparisonResult(Network *obj1, Network *obj2) {
        if(obj1.nwid > obj2.nwid) {
            return true;
        }
        return false;
    }];

    @synchronized(_allNetworks) {
        _allNetworks = networks;
    }

    [self saveNetworks];

    NSDictionary *userInfo = [NSDictionary dictionaryWithObject:networks forKey:@"networks"];

    [[NSNotificationCenter defaultCenter] postNotificationName:NetworkUpdateKey
                                                        object:nil
                                                      userInfo:userInfo];
}

- (NSInteger)findNetworkWithID:(UInt64)networkId
{
    for(int i = 0; i < [_allNetworks count]; ++i) {
        Network *nw = [_allNetworks objectAtIndex:i];

        if(nw.nwid == networkId) {
            return i;
        }
    }

    return NSNotFound;
}


- (NSInteger)findSavedNetworkWithID:(UInt64)networkId
{
    for(int i = 0; i < [_savedNetworks count]; ++i) {
        Network *nw = [_savedNetworks objectAtIndex:i];

        if(nw.nwid == networkId) {
            return i;
        }
    }

    return NSNotFound;
}

- (void)saveNetworks
{
    NSString *filePath = [self dataFile];

    @synchronized(_allNetworks) {
        [NSKeyedArchiver archiveRootObject:_allNetworks toFile:filePath];
    }
}

@end
