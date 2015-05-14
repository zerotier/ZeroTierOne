//
//  MenuItemProxy.h
//  MacGap
//
//  Created by Joe Hildebrand on 1/15/12.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import "Command.h"
#import "CallbackDelegate.h"

@class MenuProxy;

@interface MenuItemProxy : Command {
    NSMenuItem *item;
    CallbackDelegate *callback;
}

+ (MenuItemProxy*) proxyWithContext:(JSContextRef)aContext andMenuItem:(NSMenuItem*)anItem;

- (MenuProxy*)addSubmenu;

- (void) remove;
- (void) setCallback:(WebScriptObject*)aCallback;
- (void) setKey:(NSString*)keyCommand;
- (void) setTitle:(NSString*)title;
- (void) enable;
- (void) disable;
- (MenuProxy*)submenu;

@end
