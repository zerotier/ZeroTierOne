//
//  MenuItemProxy.m
//  MacGap
//
//  Created by Joe Hildebrand on 1/15/12.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import "MenuItemProxy.h"
#import "MenuProxy.h"

@implementation MenuItemProxy

- (id) initWithContext:(JSContextRef)aContext andMenuItem:(NSMenuItem*)anItem
{
    NSAssert(anItem, @"anItem required");
    self = [super initWithContext:aContext];
    if (!self)
        return nil;
    item = anItem;
    item.representedObject = self;

    return self;
}

+ (MenuItemProxy*) proxyWithContext:(JSContextRef)aContext andMenuItem:(NSMenuItem*)anItem
{
    MenuItemProxy *proxy = [anItem representedObject];
    if (proxy)
    {
        NSLog(@"MIP Cache hit");
        NSAssert([proxy class] == [MenuItemProxy class], @"Bad proxy");
        return proxy;
    }
    return [[MenuItemProxy alloc] initWithContext:aContext andMenuItem:anItem];
}

- (NSString*) description
{
    return [item description];
}

- (MenuProxy*)addSubmenu
{
    NSMenu *s = [item submenu];
    if (!s)
    {
        s = [[NSMenu alloc] initWithTitle:@"FFFFFFOOOOO"];
        [item setSubmenu:s];
    }
    return [MenuProxy proxyWithContext:context andMenu:s];
}

- (void) remove
{
    NSMenu *menu = [item menu];
    [menu removeItem:item];
}

- (void)callCallback:(id)sender
{
    [callback callWithParams:[sender title], nil];
}

- (void) setCallback:(WebScriptObject*)aCallback
{
    NSAssert(item, @"item required");
    callback = [[CallbackDelegate alloc] initWithContext:context forCallback:aCallback];
    [item setAction:@selector(callCallback:)];
    [item setTarget:self];
}

- (void)setKey:(NSString*)keyCommand
{
    NSString *aKey = [MenuProxy getKeyFromString:keyCommand];
    [item setKeyEquivalent:aKey];
    
    NSUInteger modifiers = [MenuProxy getModifiersFromString:keyCommand];
    [item setKeyEquivalentModifierMask:modifiers];
}

- (void) setTitle:(NSString*)title
{
    [item setTitle:title];
}

- (MenuProxy*)submenu;
{
    // TODO: make this work as a property
    NSMenu *s = [item submenu];
    if (!s)
        return nil;
    return [MenuProxy proxyWithContext:context andMenu:s];
}

- (void) enable
{
    [item setEnabled:YES];
}

- (void) disable
{
    [item setEnabled:NO];
}

#pragma mark WebScripting protocol

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
	
    if (selector == @selector(addSubmenu)) {
		result = @"addSubmenu";
	}
	else if (selector == @selector(remove)) {
		result = @"remove";
	}
	else if (selector == @selector(setCallback:)) {
		result = @"setCallback";
	}
	else if (selector == @selector(setKey:)) {
		result = @"setKey";
	}
	else if (selector == @selector(setTitle:)) {
		result = @"setTitle";
	}
	else if (selector == @selector(submenu)) {
		result = @"submenu";
	}
    else if (selector == @selector(enable)) {
		result = @"enable";
	}
    else if (selector == @selector(disable)) {
		result = @"disable";
	}
	
	return result;
}

@end
