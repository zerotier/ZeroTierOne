//
//  CallbackDelegate.h
//  MacGap
//
//  Created by Joe Hildebrand on 1/10/12.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import "Command.h"

@interface CallbackDelegate : Command {
}

@property JSObjectRef callback;

- (id) initWithContext:(JSContextRef)aContext forCallback:(WebScriptObject*)aCallback;
- (id) call;
- (id) callWithParams:(id)firstOrNil, ... NS_REQUIRES_NIL_TERMINATION;

@end
