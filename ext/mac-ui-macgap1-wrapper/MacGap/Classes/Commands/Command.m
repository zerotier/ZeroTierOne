//
//  Command.m
//  MacGap
//
//  Created by Joe Hildebrand on 1/10/12.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import "Command.h"
#import <JavaScriptCore/JSContextRef.h>

@implementation Command 

- (id) initWithContext:(JSContextRef)aContext {
    self = [super init];
    if (!self)
        return nil;
    context = aContext;
    JSGlobalContextRetain((JSGlobalContextRef)context);
    return self;
}

- (void)dealloc
{
    if (context)
        JSGlobalContextRelease((JSGlobalContextRef)context);
}
@end
