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

#import <Foundation/Foundation.h>

#import "AuthtokenCopy.h"


NSString* getAdminAuthToken(AuthorizationRef authRef) {
    char *tool = "/bin/cat";
    char *args[] = { "/Library/Application Support/ZeroTier/One/authtoken.secret", NULL};
    FILE *pipe = nil;
    char token[25];
    memset(token, 0, sizeof(char)*25);


    OSStatus status = AuthorizationExecuteWithPrivileges(authRef, tool, kAuthorizationFlagDefaults, args, &pipe);

    if (status != errAuthorizationSuccess) {
        NSLog(@"Reading authtoken failed!");


        switch(status) {
            case errAuthorizationDenied:
                NSLog(@"Autorization Denied");
                break;
            case errAuthorizationCanceled:
                NSLog(@"Authorization Canceled");
                break;
            case errAuthorizationInternal:
                NSLog(@"Authorization Internal");
                break;
            case errAuthorizationBadAddress:
                NSLog(@"Bad Address");
                break;
            case errAuthorizationInvalidRef:
                NSLog(@"Invalid Ref");
                break;
            case errAuthorizationInvalidSet:
                NSLog(@"Invalid Set");
                break;
            case errAuthorizationInvalidTag:
                NSLog(@"Invalid Tag");
                break;
            case errAuthorizationInvalidFlags:
                NSLog(@"Invalid Flags");
                break;
            case errAuthorizationInvalidPointer:
                NSLog(@"Invalid Pointer");
                break;
            case errAuthorizationToolExecuteFailure:
                NSLog(@"Tool Execute Failure");
                break;
            case errAuthorizationToolEnvironmentError:
                NSLog(@"Tool Environment Failure");
                break;
            case errAuthorizationExternalizeNotAllowed:
                NSLog(@"Externalize Not Allowed");
                break;
            case errAuthorizationInteractionNotAllowed:
                NSLog(@"Interaction Not Allowed");
                break;
            case errAuthorizationInternalizeNotAllowed:
                NSLog(@"Internalize Not Allowed");
                break;
            default:
                NSLog(@"Unknown Error");
                break;
        }

        return @"";
    }

    if(pipe != nil) {
        fread(&token, sizeof(char), 24, pipe);
        fclose(pipe);

        return [NSString stringWithUTF8String:token];
    }

    return @"";
}
