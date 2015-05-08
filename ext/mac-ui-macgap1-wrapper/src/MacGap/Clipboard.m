//
//  clipboard.m
//  MacGap
//
//  Created by David Zorychta on 2013-07-22.
//  Copyright (c) 2013 Twitter. All rights reserved.
//

#import "Clipboard.h"

@implementation Clipboard

- (void) copy:(NSString*)text {
    [[NSPasteboard generalPasteboard] clearContents];
    [[NSPasteboard generalPasteboard] setString:text  forType:NSStringPboardType];
}

- (NSString *) paste {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *classArray = [NSArray arrayWithObject:[NSString class]];
    NSDictionary *options = [NSDictionary dictionary];
    BOOL ok = [pasteboard canReadObjectForClasses:classArray options:options];
    if (ok) {
        NSArray *objectsToPaste = [pasteboard readObjectsForClasses:classArray options:options];
        return (NSString *) [objectsToPaste objectAtIndex:0];
    }
    return @"";
}

+ (NSString*) webScriptNameForSelector:(SEL)selector
{
	id	result = nil;
	
	if (selector == @selector(copy:)) {
        result = @"copy";
    }
	
	return result;
}

+ (BOOL) isSelectorExcludedFromWebScript:(SEL)selector
{
    return NO;
}

+ (BOOL) isKeyExcludedFromWebScript:(const char*)name
{
	return YES;
}

@end
