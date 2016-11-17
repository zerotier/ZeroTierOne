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

enum NetworkStatus {
    REQUESTING_CONFIGURATION,
    OK,
    ACCESS_DENIED,
    NOT_FOUND,
    PORT_ERROR,
    CLIENT_TOO_OLD,
};

enum NetworkType {
    PUBLIC,
    PRIVATE,
};

@interface Network : NSObject <NSCoding>

@property (readonly) NSArray<NSString*> *assignedAddresses;
@property (readonly) BOOL bridge;
@property (readonly) BOOL broadcastEnabled;
@property (readonly) BOOL dhcp;
@property (readonly) NSString *mac;
@property (readonly) int mtu;
@property (readonly) int netconfRevision;
@property (readonly) NSString *name;
@property (readonly) UInt64 nwid;
@property (readonly) NSString *portDeviceName;
@property (readonly) int portError;
@property (readonly) enum NetworkStatus status;
@property (readonly) enum NetworkType type;
@property (readonly) BOOL allowManaged;
@property (readonly) BOOL allowGlobal;
@property (readonly) BOOL allowDefault;
@property (readonly) BOOL connected; // not persisted.  set to YES if loaded via json

- (id)initWithJsonData:(NSDictionary*)jsonData;
- (id)initWithCoder:(NSCoder *)aDecoder;
- (void)encodeWithCoder:(NSCoder *)aCoder;
+ (BOOL)defaultRouteExists:(NSArray<Network *>*)netList;
- (NSString*)statusString;
- (NSString*)typeString;

- (BOOL)hasSameNetworkId:(UInt64)networkId;

- (BOOL)isEqualToNetwork:(Network*)network;
- (BOOL)isEqual:(id)object;
- (NSUInteger)hash;

@end
