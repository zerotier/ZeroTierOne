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
			int op; /* ZT_VirtualNetworkConfigOperation */
			ZT_VirtualNetworkConfig conf;
		} nconf;
	} data;
};

typedef struct ZT_GoNodeEvent_Impl ZT_GoNodeEvent;

#ifndef __cplusplus
extern "C" {
#endif

ZT_GoNode *ZT_GoNode_new(
	int (*goPathCheckFunc)(ZT_GoNode *,ZT_Node *,uint64_t ztAddress,const void *),
	int (*goPathLookupFunc)(ZT_GoNode *,ZT_Node *,int desiredAddressFamily,void *),
	int (*goStateObjectGetFunc)(ZT_GoNode *,ZT_Node *,int objType,const uint64_t id[2],void *buf,unsigned int bufSize)
);

void ZT_GoNode_delete(ZT_GoNode *gn);

ZT_Node *ZT_GoNode_getNode(ZT_GoNode *gn);

int ZT_GoNode_phyStartListen(ZT_GoNode *gn,const char *dev,const char *ip,const int port);

int ZT_GoNode_phyStopListen(ZT_GoNode *gn,const char *dev,const char *ip,const int port);

int ZT_GoNode_waitForEvent(ZT_GoNode *gn,ZT_GoNodeEvent *ev);

#ifndef __cplusplus
}
#endif

#endif
