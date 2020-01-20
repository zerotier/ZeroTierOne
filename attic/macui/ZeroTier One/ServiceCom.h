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

@class NodeStatus;
@class Network;

@interface ServiceCom : NSObject
{
    NSString *baseURL;
    NSURLSession *session;
    BOOL _isQuitting;
    BOOL _resetKey;
}
+ (ServiceCom*)sharedInstance;

- (id)init;

- (void)getNetworklist:(void (^)(NSArray<Network*>*))completionHandler error:(NSError* __autoreleasing *)error;
- (void)getNodeStatus:(void (^)(NodeStatus*))completionHandler error:(NSError*__autoreleasing*)error;
- (void)joinNetwork:(NSString*)networkId allowManaged:(BOOL)allowManaged allowGlobal:(BOOL)allowGlobal allowDefault:(BOOL)allowDefault error:(NSError*__autoreleasing*)error;
- (void)leaveNetwork:(NSString*)networkId error:(NSError*__autoreleasing*)error;

@end
