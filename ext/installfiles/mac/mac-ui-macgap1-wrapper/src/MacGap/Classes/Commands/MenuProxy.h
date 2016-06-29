//
//  MenuProxy.h
//  MacGap
//
//  Created by Joe Hildebrand on 1/14/12.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import "Command.h"

@class MenuItemProxy;

@interface MenuProxy : Command {
    NSMenu *menu;
}

+ (MenuProxy*)proxyWithContext:(JSContextRef)aContext andMenu:(NSMenu*)aMenu;

- (MenuItemProxy*)addItemWithTitle:(NSString*)title
                     keyEquivalent:(NSString*)aKey
                          callback:(WebScriptObject*)aCallback
                           atIndex:(NSInteger)index;

- (MenuItemProxy*)addSeparator;
- (MenuItemProxy*)itemForKey:(id)key;
- (MenuProxy*)removeItem:(id)key;

+ (NSString*)getKeyFromString:(NSString*)keyCommand;
+ (NSUInteger*)getModifiersFromString:(NSString*)keyCommand;

@end
