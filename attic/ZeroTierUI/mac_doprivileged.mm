#include <string.h>
#include <stdio.h>

#include "mac_doprivileged.h"

#undef slots
#include <Cocoa/Cocoa.h>

bool macExecutePrivilegedShellCommand(const char *commandAndArgs)
{
	char tmp[32768];

	snprintf(tmp,sizeof(tmp),"do shell script \"%s\" with administrator privileges\n",commandAndArgs);
	tmp[32767] = (char)0;

	NSString *scriptApple = [[NSString alloc] initWithUTF8String:tmp];
	NSAppleScript *as = [[NSAppleScript alloc] initWithSource:scriptApple];
	NSDictionary *err = nil;
	[as executeAndReturnError:&err];
	[as release];
	[scriptApple release];

	return (err == nil);
}
