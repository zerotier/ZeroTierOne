/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_GONODE_H
#define ZT_GONODE_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../core/zerotier.h"
#include "../core/Constants.hpp"

#ifdef __cplusplus
extern "C" {
#endif

typedef void ZT_GoTap;
struct ZT_GoNode_Impl;
typedef struct ZT_GoNode_Impl ZT_GoNode;

extern const char *const ZT_PLATFORM_DEFAULT_HOMEPATH;

ZT_GoNode *ZT_GoNode_new(const char *workingPath,uintptr_t userPtr);
void ZT_GoNode_delete(ZT_GoNode *gn);
ZT_Node *ZT_GoNode_getNode(ZT_GoNode *gn);
int ZT_GoNode_phyStartListen(ZT_GoNode *gn,const char *dev,const char *ip,int port,int primary);
int ZT_GoNode_phyStopListen(ZT_GoNode *gn,const char *dev,const char *ip,int port);
ZT_GoTap *ZT_GoNode_join(ZT_GoNode *gn,uint64_t nwid,const ZT_Fingerprint *controllerFingerprint);
void ZT_GoNode_leave(ZT_GoNode *gn,uint64_t nwid);

void ZT_GoTap_setEnabled(ZT_GoTap *tap,int enabled);
int ZT_GoTap_addIp(ZT_GoTap *tap,int af,const void *ip,int netmaskBits);
int ZT_GoTap_removeIp(ZT_GoTap *tap,int af,const void *ip,int netmaskBits);
int ZT_GoTap_ips(ZT_GoTap *tap,void *buf,unsigned int bufSize);
void ZT_GoTap_deviceName(ZT_GoTap *tap,char nbuf[256]);
void ZT_GoTap_setFriendlyName(ZT_GoTap *tap,const char *friendlyName);
void ZT_GoTap_setMtu(ZT_GoTap *tap,unsigned int mtu);

int ZT_isTemporaryV6Address(const char *ifname,const struct sockaddr_storage *a);

void *ZT_malloc(unsigned long s);

#ifdef __cplusplus
}
#endif

#endif
