//
//  NodeStatus.m
//  ZeroTier One
//
//  Created by Grant Limberg on 8/4/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import "NodeStatus.h"

@implementation NodeStatus

- (id)initWithJsonData:(NSDictionary*)jsonData
{
    self = [super init];

    if(self) {
        _address = (NSString*)[jsonData objectForKey:@"address"];
        _publicIdentity = (NSString*)[jsonData objectForKey:@"publicIdentity"];
        _online = [(NSNumber*)[jsonData objectForKey:@"online"] boolValue];
        _tcpFallbackActive = [(NSNumber*)[jsonData objectForKey:@"tcpFallbackActive"] boolValue];
        _versionMajor = [(NSNumber*)[jsonData objectForKey:@"versionMajor"] intValue];
        _versionMinor = [(NSNumber*)[jsonData objectForKey:@"versionMinor"] intValue];
        _versionRev = [(NSNumber*)[jsonData objectForKey:@"versionRev"] intValue];
        _version = (NSString*)[jsonData objectForKey:@"version"];
        _clock = [(NSNumber*)[jsonData objectForKey:@"clock"] unsignedLongLongValue];
    }

    return self;
}
@end
