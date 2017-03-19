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
