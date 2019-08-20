#define MINIUPNP_STATICLIB
#define MINIUPNPC_SET_SOCKET_TIMEOUT
#define MINIUPNPC_GET_SRC_ADDR
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 600
#define MINIUPNPC_VERSION_STRING "2.0"
#define UPNP_VERSION_STRING "UPnP/1.1"

#ifdef __LINUX__
#define OS_STRING "Linux"
#endif
#ifdef __APPLE__
#define OS_STRING "Darwin"
#endif
#ifdef __WINDOWS__
#define OS_STRING "Windows"
#endif
#ifndef OS_STRING
#define OS_STRING "ZeroTier"
#endif

#ifdef __APPLE__
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#endif

#include "../ext/miniupnpc/connecthostport.c"
#include "../ext/miniupnpc/igd_desc_parse.c"
#include "../ext/miniupnpc/minisoap.c"
#include "../ext/miniupnpc/miniupnpc.c"
#include "../ext/miniupnpc/miniwget.c"
#include "../ext/miniupnpc/minixml.c"
#include "../ext/miniupnpc/portlistingparse.c"
#include "../ext/miniupnpc/receivedata.c"
#include "../ext/miniupnpc/upnpcommands.c"
#include "../ext/miniupnpc/upnpdev.c"
#include "../ext/miniupnpc/upnperrors.c"
#include "../ext/miniupnpc/upnpreplyparse.c"
#include "../ext/miniupnpc/minissdpc.c"
