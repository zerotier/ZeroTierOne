//
//  NodeStatus.h
//  ZeroTier One
//
//  Created by Grant Limberg on 8/4/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NodeStatus : NSObject

@property (readonly) NSString *address;
@property (readonly) NSString *publicIdentity;
@property (readonly) BOOL online;
@property (readonly) BOOL tcpFallbackActive;
@property (readonly) int versionMajor;
@property (readonly) int versionMinor;
@property (readonly) int versionRev;
@property (readonly) NSString *version;
@property (readonly) UInt64 clock;

- (id)initWithJsonData:(NSDictionary*)jsonData;

@end
