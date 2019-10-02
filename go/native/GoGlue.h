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
#include "../../node/Constants.hpp"

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

extern const char *ZT_PLATFORM_DEFAULT_HOMEPATH;

/****************************************************************************/

ZT_GoNode *ZT_GoNode_new(const char *workingPath,uintptr_t userPtr);

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

int ZT_GoTap_addIp(ZT_GoTap *tap,int af,const void *ip,int netmaskBits);

int ZT_GoTap_removeIp(ZT_GoTap *tap,int af,const void *ip,int netmaskBits);

/* The buf buffer is filled with tuplies of:
 *   uint8_t family
 *   uint8_t ip[4 or 16]
 *   uint8_t netmask bits (up to 32 for ipv4, 128 for ipv6)
 *
 * This function returns the number of such tuples in the result.
 * If the buffer isn't big enough results are incomplete.
 */
int ZT_GoTap_ips(ZT_GoTap *tap,void *buf,unsigned int bufSize);

void ZT_GoTap_deviceName(ZT_GoTap *tap,char nbuf[256]);

void ZT_GoTap_setFriendlyName(ZT_GoTap *tap,const char *friendlyName);

void ZT_GoTap_setMtu(ZT_GoTap *tap,unsigned int mtu);

int ZT_GoTap_addRoute(ZT_GoTap *tap,int targetAf,const void *targetIp,int targetNetmaskBits,int viaAf,const void *viaIp,unsigned int metric);

int ZT_GoTap_removeRoute(ZT_GoTap *tap,int targetAf,const void *targetIp,int targetNetmaskBits,int viaAf,const void *viaIp,unsigned int metric);

/****************************************************************************/

const char *ZT_GoIdentity_generate(int type);

int ZT_GoIdentity_validate(const char *idStr);

int ZT_GoIdentity_sign(const char *idStr,const void *data,unsigned int len,void *sigbuf,unsigned int sigbuflen);

int ZT_GoIdentity_verify(const char *idStr,const void *data,unsigned int len,const void *sig,unsigned int siglen);

/****************************************************************************/

struct ZT_GoLocator_Info {
	char id[1024];
	unsigned int phyCount;
	unsigned int virtCount;
	struct sockaddr_storage phy[256];
	char virt[256][1024];
};

/* Returns length of private key stored in private key buffer on success, -1 on fail */
int ZT_GoLocator_makeSecureDNSName(char name[256],unsigned int nameBufSize,uint8_t *privateKey,unsigned int privateKeyBufSize);

/*
 * The id is the full identity described by the locator. It must include
 * its secret key to permit the locator to be signed.
 *
 * Physical addresses must be IPv4 or IPv6 IP/port pairs. Virtual addresses
 * must be full ZeroTier identities in string format.
 *
 * On success this returns the actual number of bytes stored in the buffer.
 * On failure -1 is returned.
 */
int ZT_GoLocator_makeLocator(
	uint8_t *buf,
	unsigned int bufSize,
	int64_t ts,
	const char *id,
	const struct sockaddr_storage *physicalAddresses,
	unsigned int physicalAddressCount,
	const char **virtualAddresses,
	unsigned int virtualAddressCount);

/* Returns >0 on success, fills info structure */
int ZT_GoLocator_decodeLocator(const uint8_t *locatorBytes,unsigned int locatorSize,struct ZT_GoLocator_Info *info);

/*
 * The privateKey and privateKeySize are those created by makeSecureDNSName.
 * Results is filled and the number of lines of TXT are returned. The value
 * -1 is returned on error.
 */
int ZT_GoLocator_makeSignedTxtRecords(
	const uint8_t *locator,
	unsigned int locatorSize,
	const char *name,
	const uint8_t *privateKey,
	unsigned int privateKeySize,
	char results[256][256]);

/****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
