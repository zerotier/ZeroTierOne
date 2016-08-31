//
//  JoinNetworkViewController.m
//  ZeroTier One
//
//  Created by Grant Limberg on 8/7/16.
//  Copyright © 2016 ZeroTier, Inc. All rights reserved.
//

#import "JoinNetworkViewController.h"
#import "ServiceCom.h"



NSString * const JoinedNetworksKey = @"com.zerotier.one.joined-networks";

@interface NSString (extra)

- (BOOL)contains:(NSString*)find;
//- (NSString*)trunc:(NSInteger)length trailing:(NSString*)trailing;

@end

@implementation NSString (extra)

- (BOOL)contains:(NSString*)find {
    NSRange range = [self rangeOfString:find];
    return range.location != NSNotFound;
}
//
//- (NSString*)trunc:(NSInteger)length trailing:(NSString*)trailing {
//
//}

@end


@implementation JoinNetworkViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do view setup here.
    [self.network setDelegate:self];
    [self.network setDataSource:self];
}

- (void)viewWillAppear {
    [super viewWillAppear];

    self.allowManagedCheckBox.state = NSOnState;
    self.allowGlobalCheckBox.state = NSOffState;
    self.allowDefaultCheckBox.state = NSOffState;

    self.network.stringValue = @"";

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    NSMutableArray<NSString*> *vals = [[defaults stringArrayForKey:JoinedNetworksKey] mutableCopy];

    if(vals) {
        self.values = vals;
    }
}

- (void)viewWillDisappear {
    [super viewWillDisappear];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    [defaults setObject:self.values forKey:JoinedNetworksKey];
}

- (IBAction)onJoinClicked:(id)sender {
    NSString *networkId = self.network.stringValue;

    NSError *error = nil;
    [[ServiceCom sharedInstance] joinNetwork:networkId
                                allowManaged:(self.allowManagedCheckBox.state == NSOnState)
                                 allowGlobal:(self.allowGlobalCheckBox.state == NSOnState)
                                allowDefault:(self.allowDefaultCheckBox.state == NSOnState)
                                       error:&error];

    if(error) {
        // TODO: display error message
        return;
    }

    self.network.stringValue = @"";

    if(![self.values containsObject:networkId]) {
        [self.values insertObject:networkId atIndex:0];

        while([self.values count] > 20) {
            [self.values removeLastObject];
        }
    }
}

// NSComboBoxDelegate methods

- (void)controlTextDidChange:(NSNotification *)obj {
    NSComboBox *cb = (NSComboBox*)obj.object;
    NSString *value = cb.stringValue;

    NSString *allowedCharacters = @"abcdefABCDEF0123456789";

    NSString *outValue = @"";

    for(int i = 0; i < [value length]; ++i) {
        if(![allowedCharacters contains:[NSString stringWithFormat:@"%C", [value characterAtIndex:i]]]) {
            NSBeep();
        }
        else {
            outValue = [outValue stringByAppendingString:[NSString stringWithFormat:@"%C", [value characterAtIndex:i]]];
        }
    }

    if([outValue lengthOfBytesUsingEncoding:NSUTF8StringEncoding] == 16) {
        self.joinButton.enabled = YES;
    }
    else {
        if([outValue lengthOfBytesUsingEncoding:NSUTF8StringEncoding] > 16) {
            NSRange range = {0, 16};
            range = [outValue rangeOfComposedCharacterSequencesForRange:range];
            outValue = [outValue substringWithRange:range];
            NSBeep();
            self.joinButton.enabled = YES;
        }
        else {
            self.joinButton.enabled = NO;
        }
    }

    cb.stringValue = outValue;
}

// end NSComboBoxDelegate methods

// NSComboBoxDataSource methods

- (NSInteger)numberOfItemsInComboBox:(NSComboBox *)aComboBox {
    return [self.values count];
}

- (id)comboBox:(NSComboBox *)aComboBox objectValueForItemAtIndex:(NSInteger)index {
    return [self.values objectAtIndex:index];
}

- (NSUInteger)comboBox:(NSComboBox *)aComboBox indexOfItemWithStringValue:(NSString *)string {
    NSUInteger counter = 0;

    for(NSString *val in self.values) {
        if([val isEqualToString:string]) {
            return counter;
        }

        counter += 1;
    }

    return NSNotFound;
}

- (NSString*)comboBox:(NSComboBox *)aComboBox completedString:(NSString *)string {
    for(NSString *val in self.values) {
        if([val hasPrefix:string]) {
            return val;
        }
    }
    return nil;
}

// end NSComboBoxDataSource methods

@end
