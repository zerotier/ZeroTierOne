//
//  Network.h
//  ZeroTier One
//
//  Created by Grant Limberg on 8/4/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

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

@end
