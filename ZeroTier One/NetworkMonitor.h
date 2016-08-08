//
//  NetworkMonitor.h
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

extern NSString * const NetworkUpdateKey;
extern NSString * const StatusUpdateKey;

@class Network;

@interface NetworkMonitor : NSObject
{
    NSMutableArray<Network*> *_savedNetworks;
    NSArray<Network*> *_receivedNetworks;
    NSMutableArray<Network*> *_allNetworks;

    NSTimer *_timer;
}

- (id)init;
- (void)dealloc;

- (void)start;
- (void)stop;

- (void)updateNetworkInfo;

- (void)deleteSavedNetwork:(NSString*)networkId;

@end
