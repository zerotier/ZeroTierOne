//
//  Command.h
//  MacGap
//
//  Created by Joe Hildebrand on 1/10/12.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Webkit/WebScriptObject.h>

@interface Command : NSObject {
    JSContextRef context;
}

- (id) initWithContext:(JSContextRef)aContext;

@end
