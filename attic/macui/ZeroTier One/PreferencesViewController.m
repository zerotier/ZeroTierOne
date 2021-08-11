/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#import "PreferencesViewController.h"

@interface PreferencesViewController ()

@end

@implementation PreferencesViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    if([self isLaunchAtStartup]) {
        self.startupCheckBox.state = NSOnState;
    }
    else {
        self.startupCheckBox.state = NSOffState;
    }
}

- (IBAction)onStartupCheckBoxChanged:(NSButton *)sender
{
    if(sender.state == NSOnState) {
        [self setLaunchAtLoginEnabled:YES];
    }
    else {
        [self setLaunchAtLoginEnabled:NO];
    }

}

- (void)setLaunchAtLoginEnabled:(BOOL)enabled
{
     LSSharedFileListRef loginItemsRef = LSSharedFileListCreate(NULL, kLSSharedFileListSessionLoginItems, NULL);

    if (enabled) {
        // Add the app to the LoginItems list.
        CFURLRef appUrl = (__bridge CFURLRef)[NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
        LSSharedFileListItemRef itemRef = LSSharedFileListInsertItemURL(loginItemsRef, kLSSharedFileListItemLast, NULL, NULL, appUrl, NULL, NULL);
        if (itemRef) CFRelease(itemRef);
    }
    else {
        // Remove the app from the LoginItems list.
        LSSharedFileListItemRef itemRef = [self itemRefInLoginItems];
        LSSharedFileListItemRemove(loginItemsRef,itemRef);
        if (itemRef != nil) CFRelease(itemRef);
    }
}


- (BOOL)isLaunchAtStartup {
    // See if the app is currently in LoginItems.
    LSSharedFileListItemRef itemRef = [self itemRefInLoginItems];
    // Store away that boolean.
    BOOL isInList = itemRef != nil;
    // Release the reference if it exists.
    if (itemRef != nil) CFRelease(itemRef);

    return isInList;
}

- (LSSharedFileListItemRef)itemRefInLoginItems {
    LSSharedFileListItemRef itemRef = nil;

    NSString * appPath = [[NSBundle mainBundle] bundlePath];

    // This will retrieve the path for the application
    // For example, /Applications/test.app
    CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:appPath];

    // Create a reference to the shared file list.
    LSSharedFileListRef loginItems = LSSharedFileListCreate(NULL, kLSSharedFileListSessionLoginItems, NULL);

    if (loginItems) {
        UInt32 seedValue;
        //Retrieve the list of Login Items and cast them to
        // a NSArray so that it will be easier to iterate.
        NSArray  *loginItemsArray = (__bridge NSArray *)LSSharedFileListCopySnapshot(loginItems, &seedValue);
        for(int i = 0; i< [loginItemsArray count]; i++){
            LSSharedFileListItemRef currentItemRef = (__bridge LSSharedFileListItemRef)[loginItemsArray
                                                                                        objectAtIndex:i];
            //Resolve the item with URL
            if (LSSharedFileListItemResolve(currentItemRef, 0, (CFURLRef*) &url, NULL) == noErr) {
                NSString * urlPath = [(__bridge NSURL*)url path];
                if ([urlPath compare:appPath] == NSOrderedSame){
                    itemRef = currentItemRef;
                }
            }
        }
    }    
    CFRelease(loginItems);
    return itemRef;
}

@end
