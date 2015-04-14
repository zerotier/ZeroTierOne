#ifndef mac_doprivileged_h
#define mac_doprivileged_h

#ifdef __APPLE__

// commandAndArgs can contain only single-tic quotes and should redirect its
// stdout and stderr somewhere...
bool macExecutePrivilegedShellCommand(const char *commandAndArgs);

#endif

#endif
