#include "../core/Constants.hpp"
#include "rust-osdep.h"

#ifdef __APPLE__
#ifndef SIOCAUTOCONF_START
#define SIOCAUTOCONF_START _IOWR('i', 132, struct in6_ifreq)    /* accept rtadvd on this interface */
#endif
#ifndef SIOCAUTOCONF_STOP
#define SIOCAUTOCONF_STOP _IOWR('i', 133, struct in6_ifreq)    /* stop accepting rtadv for this interface */
#endif
#endif

extern "C" {

#ifdef __APPLE__
extern const unsigned long c_BIOCSBLEN = BIOCSBLEN;
extern const unsigned long c_BIOCIMMEDIATE = BIOCIMMEDIATE;
extern const unsigned long c_BIOCSSEESENT = BIOCSSEESENT;
extern const unsigned long c_BIOCSETIF = BIOCSETIF;
extern const unsigned long c_BIOCSHDRCMPLT = BIOCSHDRCMPLT;
extern const unsigned long c_BIOCPROMISC = BIOCPROMISC;
extern const unsigned long c_SIOCGIFINFO_IN6 = SIOCGIFINFO_IN6;
extern const unsigned long c_SIOCSIFINFO_FLAGS = SIOCSIFINFO_FLAGS;
extern const unsigned long c_SIOCAUTOCONF_START = SIOCAUTOCONF_START;
extern const unsigned long c_SIOCAUTOCONF_STOP = SIOCAUTOCONF_STOP;
#endif

}
