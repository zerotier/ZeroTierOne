//
//  CallbackDelegate.m
//  MacGap
//
//  Created by Joe Hildebrand on 1/10/12.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import "CallbackDelegate.h"
#import <JavaScriptCore/JavaScript.h>

@implementation CallbackDelegate

@synthesize callback;

- (id) initWithContext:(JSContextRef)aContext forCallback:(WebScriptObject*)aCallback
{
    if (!aCallback)
        return nil;
    if ([aCallback isKindOfClass:[WebUndefined class]])
        return nil;
    
    self = [super initWithContext:aContext];
    if (!self)
        return nil;

    callback = [aCallback JSObject];
    JSValueProtect(context, callback);
    return self;
}

- (void) dealloc
{
    if (callback)
    {
        JSValueUnprotect(context, callback);
        callback = nil;
    }
}

- (id) objectFromValue:(JSValueRef)val
{
    JSStringRef jstr;
    NSString *rets;
    
    switch(JSValueGetType(context, val))
    {
        case kJSTypeUndefined:
        case kJSTypeNull:
            return nil;
        case kJSTypeBoolean:
            return [NSNumber numberWithBool:JSValueToBoolean(context, val)];
        case kJSTypeNumber:
            return [NSNumber numberWithDouble:JSValueToNumber(context, val, NULL)];
        case kJSTypeString:
            jstr = JSValueToStringCopy(context, val, NULL);
            size_t sz = JSStringGetMaximumUTF8CStringSize(jstr);
            char *buf = (char*)malloc(sz);
            JSStringGetUTF8CString(jstr, buf, sz);
            rets = [NSString stringWithUTF8String:buf];
            free(buf);
            return rets;
        case kJSTypeObject:
            // TODO: dictionary or something
            return nil;
        default:
            NSAssert(false, @"Invalid JavaScript type");
            return nil;
    }
}

- (JSValueRef) valueFromObject:(id)obj
{
    JSValueRef val = nil;
    if (!obj)
    {
        val = JSValueMakeNull(context);
    }
    else if ([obj isKindOfClass:[NSString class]])
    {
        JSStringRef jstr = JSStringCreateWithUTF8CString([obj UTF8String]);
        val = JSValueMakeString(context, jstr); 
        JSStringRelease(jstr);         
    }
    else if ([obj isKindOfClass:[NSNumber class]])
    {
        val = JSValueMakeNumber(context, [obj doubleValue]);
    }
    else if ([obj isKindOfClass:[NSDictionary class]])
    {
        JSObjectRef o = JSObjectMake(context, NULL, NULL);
        for (NSString *key in obj) 
        {
            JSStringRef kstr = JSStringCreateWithUTF8CString([key UTF8String]);
            JSValueRef v = [self valueFromObject:[obj objectForKey:key]];
            
            JSObjectSetProperty(context, o, kstr, v, kJSPropertyAttributeNone, NULL);
            JSStringRelease(kstr);         
        }
        val = o;
    }
    else if ([obj isKindOfClass:[NSArray class]])
    {
        NSUInteger pcount = [obj count];
        JSValueRef jsArgs[pcount];
        NSUInteger i=0;
        for (id v in obj)
        {
            jsArgs[i++] = [self valueFromObject:v];
        }
        val = JSObjectMakeArray(context, pcount, jsArgs, NULL);
    }
    else if ([obj isKindOfClass:[NSDate class]])
    {
        NSTimeInterval secs = [obj timeIntervalSince1970];
        JSValueRef jsArgs[1];
        // call the Date(milliseconds) constructor in JS
        jsArgs[0] = JSValueMakeNumber(context, secs * 1000.0);
        val = JSObjectMakeDate(context, 1, jsArgs, NULL);
    }
    else
    {
        NSLog(@"Warning: unknown object type for: %@", obj);
        val = JSValueMakeUndefined(context);
    }
    return val;
}

- (id) call
{
    NSAssert(callback, @"Callback required");
    if (!JSObjectIsFunction(context, callback))
        return nil;
    
    JSValueRef jsArgs[0];
    JSValueRef ret = JSObjectCallAsFunction(context, callback, NULL, 0, jsArgs, NULL);
    return [self objectFromValue:ret];    
}

- (id) callWithParams:(id)firstOrNil, ...
{
    NSAssert(callback, @"Callback required");
    if (!JSObjectIsFunction(context, callback))
        return nil;
    NSUInteger pcount = 0;
    id p;
    va_list args;
    va_start(args, firstOrNil);
    for (p=firstOrNil; p; p=va_arg(args, id))
    {
        pcount++;
    }
    va_end(args);
    
    JSValueRef jsArgs[pcount];
    NSUInteger j = 0;
    va_start(args, firstOrNil);
    for (p=firstOrNil; p; p=va_arg(args, id))
    {
        jsArgs[j++] = [self valueFromObject:p];
    }
    va_end(args);
    
    JSValueRef ret = JSObjectCallAsFunction(context, callback, NULL, j, jsArgs, NULL);
    return [self objectFromValue:ret];
}

@end
