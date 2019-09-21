/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by vergnn 2.0 of the Apache License.
 */
/****/

#ifndef ZT_GONODE_H
#define ZT_GONODE_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/ZeroTierCore.h"

/****************************************************************************/

/* A pointer to an instance of EthernetTap */
typedef void ZT_GoTap;

/* ZT_GoNode is a C struct and functions that wraps ZT_Node for use via cgo. It
 * performs UDP and other direct I/O in C for performance but otherwise lets
 * the Go code control the node's behavior. */
struct ZT_GoNode_Impl;
typedef struct ZT_GoNode_Impl ZT_GoNode;

/****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/


/****************************************************************************/

ZT_GoNode *ZT_GoNode_new(const char *workingPath);

void ZT_GoNode_delete(ZT_GoNode *gn);

ZT_Node *ZT_GoNode_getNode(ZT_GoNode *gn);

/* This can be called more than once to start multiple listener threads */
int ZT_GoNode_phyStartListen(ZT_GoNode *gn,const char *dev,const char *ip,const int port);

/* Close all listener threads for a given local IP and port */
int ZT_GoNode_phyStopListen(ZT_GoNode *gn,const char *dev,const char *ip,const int port);

ZT_GoTap *ZT_GoNode_join(ZT_GoNode *gn,uint64_t nwid);

void ZT_GoNode_leave(ZT_GoNode *gn,uint64_t nwid);

/****************************************************************************/

void ZT_GoTap_setEnabled(ZT_GoTap *tap,int enabled);

int ZT_GoTap_addIp(ZT_GoTap *tap,int af,const void *ip,int port);

int ZT_GoTap_removeIp(ZT_GoTap *tap,int af,const void *ip,int port);

/* The buf buffer is filled with tuplies of:
 *   uint8_t family
 *   uint8_t ip[4 or 16]
 *   uint16_t port (big-endian byte order)
 *
 * This function returns the number of such tuples in the result.
 * If the buffer isn't big enough results are incomplete.
 */
int ZT_GoTap_ips(ZT_GoTap *tap,void *buf,unsigned int bufSize);

void ZT_GoTap_deviceName(ZT_GoTap *tap,char nbuf[256]);

void ZT_GoTap_setFriendlyName(ZT_GoTap *tap,const char *friendlyName);

void ZT_GoTap_setMtu(ZT_GoTap *tap,unsigned int mtu);

/****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
