//
//  ServiceCom.h
//  ZeroTier One
//
//  Created by Grant Limberg on 8/4/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class NodeStatus;
@class Network;

@interface ServiceCom : NSObject
{
    NSString *baseURL;
    NSURLSession *session;
}
+ (ServiceCom*)sharedInstance;

- (id)init;

- (void)getNetworklist:(void (^)(NSArray<Network*>*))completionHandler;
- (void)getNodeStatus:(void (^)(NodeStatus*))completionHandler;
- (void)joinNetwork:(NSString*)networkId allowManaged:(BOOL)allowManaged allowGlobal:(BOOL)allowGlobal allowDefault:(BOOL)allowDefault;
- (void)leaveNetwork:(NSString*)networkId;

@end
