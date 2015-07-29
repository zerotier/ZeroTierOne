/* $Id: minissdpc.h,v 1.2 2012/09/27 15:42:10 nanard Exp $ */
/* Project: miniupnp
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * Author: Thomas Bernard
 * Copyright (c) 2005-2007 Thomas Bernard
 * This software is subjects to the conditions detailed
 * in the LICENCE file provided within this distribution */
#ifndef MINISSDPC_H_INCLUDED
#define MINISSDPC_H_INCLUDED

struct UPNPDev *
getDevicesFromMiniSSDPD(const char * devtype, const char * socketpath);

#endif

