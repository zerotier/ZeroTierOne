//
//  AppDelegate.m
//  MacGap
//
//  Created by Alex MacCaw on 08/01/2012.
//  Copyright (c) 2012 Twitter. All rights reserved.
//

#import "AppDelegate.h"
#include <sys/stat.h>
#include <sys/types.h>

@implementation AppDelegate

@synthesize windowController;

- (void) applicationWillFinishLaunching:(NSNotification *)aNotification
{
}

-(BOOL)applicationShouldHandleReopen:(NSApplication*)application
                   hasVisibleWindows:(BOOL)visibleWindows{
    if(!visibleWindows){
        [self.windowController.window makeKeyAndOrderFront: nil];
    }
    return YES;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
    return YES;
}

- (void) applicationDidFinishLaunching:(NSNotification *)aNotification { 
    char buf[16384],userAuthTokenPath[4096];
    struct stat systemAuthTokenStat,userAuthTokenStat;

    FILE *pf = fopen("/Library/Application Support/ZeroTier/One/zerotier-one.port","r");
    long port = 9993; // default
    if (pf) {
        long n = fread(buf,1,sizeof(buf)-1,pf);
        if (n > 0) {
            buf[n] = (char)0;
            port = strtol(buf,(char **)0,10);
        }
        fclose(pf);
    }

    char url[16384];
    memset(url,0,sizeof(url));

    const char *homeDir = getenv("HOME");
    if (homeDir) {
        snprintf(userAuthTokenPath,sizeof(userAuthTokenPath),"%s/Library/Application Support/ZeroTier/One/authtoken.secret",homeDir);

        bool userAuthTokenOutOfDate = false;
        memset(&systemAuthTokenStat,0,sizeof(systemAuthTokenStat));
        memset(&userAuthTokenStat,0,sizeof(userAuthTokenStat));
        if (stat("/Library/Application Support/ZeroTier/One/authtoken.secret",&systemAuthTokenStat) == 0) {
            if (stat(userAuthTokenPath,&userAuthTokenStat) == 0) {
                if (userAuthTokenStat.st_mtimespec.tv_sec < systemAuthTokenStat.st_mtimespec.tv_sec)
                    userAuthTokenOutOfDate = true;
            }
        }

        if (!userAuthTokenOutOfDate) {
            pf = fopen(userAuthTokenPath,"r");
            if (pf) {
                long n = fread(buf,1,sizeof(buf)-1,pf);
                if (n > 0) {
                    buf[n] = (char)0;
                    snprintf(url,sizeof(url),"http://127.0.0.1:%ld/index.html?authToken=%s",port,buf);
                }
                fclose(pf);
            }
        }
    }

    if (!url[0]) {
        // Create authorization reference
        OSStatus status;
        AuthorizationRef authorizationRef;
        
        // AuthorizationCreate and pass NULL as the initial
        // AuthorizationRights set so that the AuthorizationRef gets created
        // successfully, and then later call AuthorizationCopyRights to
        // determine or extend the allowable rights.
        // http://developer.apple.com/qa/qa2001/qa1172.html
        status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authorizationRef);
        if (status != errAuthorizationSuccess)
        {
            NSLog(@"Error Creating Initial Authorization: %d", status);
            return;
        }
        
        // kAuthorizationRightExecute == "system.privilege.admin"
        AuthorizationItem right = {kAuthorizationRightExecute, 0, NULL, 0};
        AuthorizationRights rights = {1, &right};
        AuthorizationFlags flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed |
        kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;
        
        // Call AuthorizationCopyRights to determine or extend the allowable rights.
        status = AuthorizationCopyRights(authorizationRef, &rights, NULL, flags, NULL);
        if (status != errAuthorizationSuccess)
        {
            NSLog(@"Copy Rights Unsuccessful: %d", status);
            return;
        }
        
        // use rm tool with -rf
        char *tool = "/bin/cat";
        char *args[] = {"/Library/Application Support/ZeroTier/One/authtoken.secret", NULL};
        FILE *pipe = NULL;
        
        status = AuthorizationExecuteWithPrivileges(authorizationRef, tool, kAuthorizationFlagDefaults, args, &pipe);
        if (status != errAuthorizationSuccess)
        {
            NSLog(@"Error: %d", status);
        }
        
        if (pipe) {
            long n = (long)fread(buf,1,sizeof(buf)-1,pipe);
            if (n > 0) {
                buf[n] = (char)0;
                snprintf(url,sizeof(url),"http://127.0.0.1:%ld/index.html?authToken=%s",port,buf);

                if (homeDir) {
                    snprintf(userAuthTokenPath,sizeof(userAuthTokenPath),"%s/Library/Application Support/ZeroTier",homeDir);
                    mkdir(userAuthTokenPath,0755);
                    snprintf(userAuthTokenPath,sizeof(userAuthTokenPath),"%s/Library/Application Support/ZeroTier/One",homeDir);
                    mkdir(userAuthTokenPath,0755);
                    snprintf(userAuthTokenPath,sizeof(userAuthTokenPath),"%s/Library/Application Support/ZeroTier/One/authtoken.secret",homeDir);
                    pf = fopen(userAuthTokenPath,"w");
                    if (pf) {
                        fwrite(buf,1,strlen(buf),pf);
                        fclose(pf);
                        chmod(userAuthTokenPath,0600);
                    }
                }
            }
            fclose(pipe);
        }

        // The only way to guarantee that a credential acquired when you
        // request a right is not shared with other authorization instances is
        // to destroy the credential.  To do so, call the AuthorizationFree
        // function with the flag kAuthorizationFlagDestroyRights.
        // http://developer.apple.com/documentation/Security/Conceptual/authorization_concepts/02authconcepts/chapter_2_section_7.html
        status = AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
    }

    NSString *urlStr = [[NSString alloc] initWithCString:url];
    self.windowController = [[WindowController alloc] initWithURL: urlStr];
    [self.windowController showWindow: [NSApplication sharedApplication].delegate];
    self.windowController.contentView.webView.alphaValue = 1.0;
    self.windowController.contentView.alphaValue = 1.0;
    [self.windowController showWindow:self];
}

@end
