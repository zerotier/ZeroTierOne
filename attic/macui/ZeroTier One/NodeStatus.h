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
