//
//  NetworkMonitor.m
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import "NetworkMonitor.h"
#import "Network.h"
#import "ServiceCom.h"
#import "NodeStatus.h"

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
        // TODO: Display error message

        [self stop];
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
        // TODO: Display error message

        [self stop];
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