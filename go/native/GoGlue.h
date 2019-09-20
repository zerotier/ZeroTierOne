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

#define ZT_GONODE_EVENT_SHUTDOWN 0
#define ZT_GONODE_EVENT_ZTEVENT 1
#define ZT_GONODE_EVENT_DNS_GET_TXT 2
#define ZT_GONODE_EVENT_STATE_PUT 3
#define ZT_GONODE_EVENT_STATE_DELETE 4
#define ZT_GONODE_EVENT_NETWORK_CONFIG_UPDATE 5

/**
 * Variant type for async core generated events pulled via waitForEvent
 */
struct ZT_GoNodeEvent_Impl
{
#ifdef __cplusplus
	inline ZT_GoNodeEvent_Impl() { memset(reinterpret_cast<void *>(this),0,sizeof(ZT_GoNodeEvent_Impl)); }
	inline ZT_GoNodeEvent_Impl(const ZT_GoNodeEvent &ev) { memcpy(reinterpret_cast<void *>(this),reinterpret_cast<const void *>(&ev),sizeof(ZT_GoNodeEvent_Impl)); }
	inline ZT_GoNodeEvent_Impl &operator=(const ZT_GoNodeEvent_Impl &ev) { memcpy(reinterpret_cast<void *>(this),reinterpret_cast<const void *>(&ev),sizeof(ZT_GoNodeEvent_Impl)); return *this; }
#endif

	int type;

	union {
		/* ZeroTier event of ZT_Event type */
		struct {
			int type;
		} zt;

		/* DNS resolution request */
		struct {
			uintptr_t requestId;
			char dnsName[256];
		} dns;

		/* State object put or delete request */
		struct {
			uint8_t data[ZT_MAX_STATE_OBJECT_SIZE];
			unsigned int len;
			int objType;
			uint64_t id[2];
		} sobj;

		/* Network configuration update event */
		struct {
			ZT_GoTap *tap;
			int op; /* ZT_VirtualNetworkConfigOperation */
			ZT_VirtualNetworkConfig conf;
		} nconf;
	} data;
};

typedef struct ZT_GoNodeEvent_Impl ZT_GoNodeEvent;

/****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/

ZT_GoNode *ZT_GoNode_new(
	const char *workingPath,
	int (*goPathCheckFunc)(ZT_GoNode *,ZT_Node *,uint64_t ztAddress,const void *),
	int (*goPathLookupFunc)(ZT_GoNode *,ZT_Node *,int desiredAddressFamily,void *),
	int (*goStateObjectGetFunc)(ZT_GoNode *,ZT_Node *,int objType,const uint64_t id[2],void *buf,unsigned int bufSize));

void ZT_GoNode_delete(ZT_GoNode *gn);

ZT_Node *ZT_GoNode_getNode(ZT_GoNode *gn);

/* This can be called more than once to start multiple listener threads */
int ZT_GoNode_phyStartListen(ZT_GoNode *gn,const char *dev,const char *ip,const int port);

/* Close all listener threads for a given local IP and port */
int ZT_GoNode_phyStopListen(ZT_GoNode *gn,const char *dev,const char *ip,const int port);

int ZT_GoNode_waitForEvent(ZT_GoNode *gn,ZT_GoNodeEvent *ev);

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

const char *ZT_GoTap_deviceName(ZT_GoTap *tap);

void ZT_GoTap_setFriendlyName(ZT_GoTap *tap,const char *friendlyName);

void ZT_GoTap_setMtu(ZT_GoTap *tap,unsigned int mtu);

/****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
