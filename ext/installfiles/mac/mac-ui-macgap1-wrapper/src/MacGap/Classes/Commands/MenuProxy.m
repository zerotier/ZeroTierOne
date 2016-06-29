//
//  MenuProxy.m
//  MacGap
//
//  Created by Joe Hildebrand on 1/14/12.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import <objc/runtime.h>
#import <JavaScriptCore/JavaScript.h>

#import "MenuProxy.h"
#import "MenuItemProxy.h"

static char REPRESENTED_OBJECT;

@interface NSMenu (represented)
@property (strong) id representedObject;
@end

@implementation NSMenu (represented)

- (id) representedObject
{
    return objc_getAssociatedObject(self, &REPRESENTED_OBJECT);
}

- (void) setRepresentedObject:(id)representedObject
{
    objc_setAssociatedObject(self, 
                             &REPRESENTED_OBJECT,
                             representedObject, 
                             OBJC_ASSOCIATION_RETAIN);
}

@end

@implementation MenuProxy

- (id) initWithContext:(JSContextRef)aContext andMenu:(NSMenu*)aMenu
{
    self = [super initWithContext:aContext];
    if (!self)
        return nil;
    menu = aMenu;
    menu.representedObject = self;
    return self;
}

+ (MenuProxy*)proxyWithContext:(JSContextRef)aContext andMenu:(NSMenu*)aMenu
{
    // singleton-ish.
    MenuProxy *ret = [aMenu representedObject];
    if (ret)
    {
        NSLog(@"MP cache hit");
        return ret;
    }
    return [[MenuProxy alloc] initWithContext:aContext andMenu:aMenu];
}

- (void) dealloc
{
    menu.representedObject = nil;
}

- (NSString*) description
{
    return [menu description];
}

static BOOL isNullish(id o)
{
    if (!o)
        return YES;
    if ([o isKindOfClass:[WebUndefined class]])
        return YES;
    return NO;
}

- (MenuItemProxy*)addItemWithTitle:(NSString*)title
                     keyEquivalent:(NSString*)keyCommand
                          callback:(WebScriptObject*)aCallback
                           atIndex:(NSInteger)index
{
    if (isNullish(title))
        title = @"";
    
    NSString *aKey = [MenuProxy getKeyFromString:keyCommand];
    NSMenuItem *item = nil;
    
    if(index) {
        item = [menu insertItemWithTitle:title action:nil keyEquivalent:aKey atIndex:index ];
    } else {
        item = [menu addItemWithTitle:title action:nil keyEquivalent:aKey ];
        
    }
    
    // Set the modifiers.
    NSUInteger modifiers = [MenuProxy getModifiersFromString:keyCommand];
    [item setKeyEquivalentModifierMask:modifiers];
   
    if(!menu.supermenu) {
        NSMenu *s = [[NSMenu alloc] initWithTitle:title];
        [item setSubmenu:s];
    }
   
    MenuItemProxy *mip = [MenuItemProxy proxyWithContext:context andMenuItem:item];
    if (!isNullish(aCallback))
        [mip setCallback:aCallback];
    
   
    return mip;
}

+ (NSString*)getKeyFromString:(NSString*)keyCommand {
    if (isNullish(keyCommand))
        keyCommand = @"";

    // Obtain the key (if there are modifiers, it will be the last character).
    NSString *aKey = @"";
    if ([keyCommand length] > 0) {
        aKey = [keyCommand substringFromIndex:[keyCommand length] - 1];
    }

    return aKey;
}

+ (NSUInteger*)getModifiersFromString:(NSString*)keyCommand {
    // aKeys may optionally specify one or more modifiers.
    NSUInteger modifiers = 0;
    
    if ([keyCommand rangeOfString:@"caps"].location != NSNotFound) modifiers += NSAlphaShiftKeyMask;
    if ([keyCommand rangeOfString:@"shift"].location != NSNotFound) modifiers += NSShiftKeyMask;
    if ([keyCommand rangeOfString:@"cmd"].location != NSNotFound) modifiers += NSCommandKeyMask;
    if ([keyCommand rangeOfString:@"ctrl"].location != NSNotFound) modifiers += NSControlKeyMask;
    if ([keyCommand rangeOfString:@"opt"].location != NSNotFound) modifiers += NSAlternateKeyMask;
    if ([keyCommand rangeOfString:@"alt"].location != NSNotFound) modifiers += NSAlternateKeyMask;

    return modifiers;
}

- (MenuItemProxy*)addSeparator
{
    NSMenuItem *sep = [NSMenuItem separatorItem];
    [menu addItem:sep];
    return [MenuItemProxy proxyWithContext:context andMenuItem:sep];
}

- (MenuItemProxy*)itemForKey:(id)key
{
    if (isNullish(key))
        return nil;
    NSMenuItem *item = nil;
    if ([key isKindOfClass:[NSNumber class]])
    {
        item = [menu itemAtIndex:[key intValue]];
    }
    else if ([key isKindOfClass:[NSString class]])
    {
        item = [menu itemWithTitle:key];
        if (!item)
        {
            // Try again, with ... appended. e.g. "Save..."
            item = [menu itemWithTitle:
                    [key stringByAppendingString:@"\u2026"]];
        }
    }
    if (!item)
        return nil;

    return [MenuItemProxy proxyWithContext:context andMenuItem:item];    
}

- (MenuProxy*)removeItem:(id)key
{
    if (isNullish(key))
        return nil;
    
    NSMenuItem *item = nil;
    if ([key isKindOfClass:[NSNumber class]])
    {
        item = [menu itemAtIndex:[key intValue]];
    }
    else if ([key isKindOfClass:[NSString class]])
    {
        item = [menu itemWithTitle:key];
        if (!item)
        {
            // Try again, with ... appended. e.g. "Save..."
            item = [menu itemWithTitle:
                    [key stringByAppendingString:@"\u2026"]];
        }
    }
    if (!item)
        return nil;
    
    [menu removeItem:item];
    return [MenuProxy proxyWithContext:context andMenu:menu];
}

+ (BOOL) isSelectorExcludedFromWebScript:(SEL)selector
{
    return [self webScriptNameForSelector:selector] == nil;
}

+ (BOOL) isKeyExcludedFromWebScript:(const char*)name
{
	return YES;
}

+ (NSString*) webScriptNameForSelector:(SEL)selector
{
	id	result = nil;
   
    if (selector == @selector(addItemWithTitle:keyEquivalent:callback:atIndex:)) {
		result = @"addItem";
	}
    else if (selector == @selector(addSeparator)) {
        result = @"addSeparator";
    }
	else if (selector == @selector(itemForKey:)) {
		result = @"getItem";
	}
    else if (selector == @selector(removeItem:)) {
		result = @"removeMenu";
	}
   
	return result;
}


@end
