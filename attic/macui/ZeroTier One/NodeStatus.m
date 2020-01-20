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
