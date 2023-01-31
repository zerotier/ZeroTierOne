/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ZT_jniutils.h"

#include "ZT_jnicache.h"

#include <string>
#include <cassert>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define LOG_TAG "Utils"

jobject createResultObject(JNIEnv *env, ZT_ResultCode code)
{
    jobject resultObject = env->CallStaticObjectMethod(ResultCode_class, ResultCode_fromInt_method, code);
    if(env->ExceptionCheck() || resultObject == NULL) {
        LOGE("Error creating ResultCode object");
        return NULL;
    }

    return resultObject;
}


jobject createVirtualNetworkStatus(JNIEnv *env, ZT_VirtualNetworkStatus status)
{
    jobject statusObject = env->CallStaticObjectMethod(VirtualNetworkStatus_class, VirtualNetworkStatus_fromInt_method, status);
    if (env->ExceptionCheck() || statusObject == NULL) {
        LOGE("Error creating VirtualNetworkStatus object");
        return NULL;
    }

    return statusObject;
}

jobject createEvent(JNIEnv *env, ZT_Event event)
{
    jobject eventObject = env->CallStaticObjectMethod(Event_class, Event_fromInt_method, event);
    if (env->ExceptionCheck() || eventObject == NULL) {
        LOGE("Error creating Event object");
        return NULL;
    }

    return eventObject;
}

jobject createPeerRole(JNIEnv *env, ZT_PeerRole role)
{
    jobject peerRoleObject = env->CallStaticObjectMethod(PeerRole_class, PeerRole_fromInt_method, role);
    if (env->ExceptionCheck() || peerRoleObject == NULL) {
        LOGE("Error creating PeerRole object");
        return NULL;
    }

    return peerRoleObject;
}

jobject createVirtualNetworkType(JNIEnv *env, ZT_VirtualNetworkType type)
{
    jobject vntypeObject = env->CallStaticObjectMethod(VirtualNetworkType_class, VirtualNetworkType_fromInt_method, type);
    if (env->ExceptionCheck() || vntypeObject == NULL) {
        LOGE("Error creating VirtualNetworkType object");
        return NULL;
    }

    return vntypeObject;
}

jobject createVirtualNetworkConfigOperation(JNIEnv *env, ZT_VirtualNetworkConfigOperation op)
{
    jobject vnetConfigOpObject = env->CallStaticObjectMethod(VirtualNetworkConfigOperation_class, VirtualNetworkConfigOperation_fromInt_method, op);
    if (env->ExceptionCheck() || vnetConfigOpObject == NULL) {
        LOGE("Error creating VirtualNetworkConfigOperation object");
        return NULL;
    }

    return vnetConfigOpObject;
}

jobject newInetAddress(JNIEnv *env, const sockaddr_storage &addr)
{
    LOGV("newInetAddress");

    jobject inetAddressObj = NULL;
    switch(addr.ss_family)
    {
        case AF_INET6:
        {
            sockaddr_in6 *ipv6 = (sockaddr_in6*)&addr;
            const unsigned char *bytes = reinterpret_cast<const unsigned char *>(&ipv6->sin6_addr.s6_addr);

            jbyteArray buff = newByteArray(env, bytes, 16);
            if(env->ExceptionCheck() || buff == NULL)
            {
                return NULL;
            }

            inetAddressObj = env->CallStaticObjectMethod(
                InetAddress_class, InetAddress_getByAddress_method, buff);
        }
        break;
        case AF_INET:
        {
            sockaddr_in *ipv4 = (sockaddr_in*)&addr;
            const unsigned char *bytes = reinterpret_cast<const unsigned char *>(&ipv4->sin_addr.s_addr);
            jbyteArray buff = newByteArray(env, bytes, 4);
            if(env->ExceptionCheck() || buff == NULL)
            {
                return NULL;
            }

            inetAddressObj = env->CallStaticObjectMethod(
                InetAddress_class, InetAddress_getByAddress_method, buff);
        }
        break;
    }
    if(env->ExceptionCheck() || inetAddressObj == NULL) {
        LOGE("Error creating InetAddress object");
        return NULL;
    }

    return inetAddressObj;
}

jobject newInetSocketAddress(JNIEnv *env, const sockaddr_storage &addr)
{
    LOGV("newInetSocketAddress Called");

    jobject inetAddressObject = NULL;

    if(addr.ss_family != 0)
    {
        inetAddressObject = newInetAddress(env, addr);

        if(env->ExceptionCheck() || inetAddressObject == NULL)
        {
            LOGE("Error creating new inet address");
            return NULL;
        }
    }
    else
    {
        return NULL;
    }

    int port = 0;
    switch(addr.ss_family)
    {
        case AF_INET6:
        {
            LOGV("IPV6 Address");
            sockaddr_in6 *ipv6 = (sockaddr_in6*)&addr;
            port = ntohs(ipv6->sin6_port);
            LOGV("Port %d", port);
        }
        break;
        case AF_INET:
        {
            LOGV("IPV4 Address");
            sockaddr_in *ipv4 = (sockaddr_in*)&addr;
            port = ntohs(ipv4->sin_port);
            LOGV("Port: %d", port);
        }
        break;
        default:
        {
            break;
        }
    }


    jobject inetSocketAddressObject = env->NewObject(InetSocketAddress_class, InetSocketAddress_ctor, inetAddressObject, port);
    if(env->ExceptionCheck() || inetSocketAddressObject == NULL) {
        LOGE("Error creating InetSocketAddress object");
    }
    return inetSocketAddressObject;
}

jobject newPeerPhysicalPath(JNIEnv *env, const ZT_PeerPhysicalPath &ppp)
{
    LOGV("newPeerPhysicalPath Called");

    jobject addressObject = newInetSocketAddress(env, ppp.address);
    if(env->ExceptionCheck() || addressObject == NULL) {
        LOGE("Error creating InetSocketAddress object");
        return NULL;
    }

    jobject pppObject = env->NewObject(
            PeerPhysicalPath_class,
            PeerPhysicalPath_ctor,
            addressObject,
            ppp.lastSend,
            ppp.lastReceive,
            ppp.preferred);
    if(env->ExceptionCheck() || pppObject == NULL)
    {
        LOGE("Error creating PPP object");
        return NULL;
    }

    return pppObject;
}

jobject newPeer(JNIEnv *env, const ZT_Peer &peer)
{
    LOGV("newPeer called");

    jobject peerObject = env->NewObject(Peer_class, Peer_ctor);
    if(env->ExceptionCheck() || peerObject == NULL)
    {
        LOGE("Error creating Peer object");
        return NULL; // out of memory
    }

    env->SetLongField(peerObject, Peer_address_field, (jlong)peer.address);
    env->SetIntField(peerObject, Peer_versionMajor_field, peer.versionMajor);
    env->SetIntField(peerObject, Peer_versionMinor_field, peer.versionMinor);
    env->SetIntField(peerObject, Peer_versionRev_field, peer.versionRev);
    env->SetIntField(peerObject, Peer_latency_field, peer.latency);
    env->SetObjectField(peerObject, Peer_role_field, createPeerRole(env, peer.role));

    jobjectArray arrayObject = newPeerPhysicalPathArray(env, peer.paths, peer.pathCount);

    env->SetObjectField(peerObject, Peer_paths_field, arrayObject);

    return peerObject;
}

jobject newNetworkConfig(JNIEnv *env, const ZT_VirtualNetworkConfig &vnetConfig)
{
    jobject vnetConfigObj = env->NewObject(VirtualNetworkConfig_class, VirtualNetworkConfig_ctor);
    if(env->ExceptionCheck() || vnetConfigObj == NULL)
    {
        LOGE("Error creating new VirtualNetworkConfig object");
        return NULL;
    }

    env->SetLongField(vnetConfigObj, VirtualNetworkConfig_nwid_field, vnetConfig.nwid);
    env->SetLongField(vnetConfigObj, VirtualNetworkConfig_mac_field, vnetConfig.mac);
    jstring nameStr = env->NewStringUTF(vnetConfig.name);
    if(env->ExceptionCheck() || nameStr == NULL)
    {
        return NULL; // out of memory
    }
    env->SetObjectField(vnetConfigObj, VirtualNetworkConfig_name_field, nameStr);

    jobject statusObject = createVirtualNetworkStatus(env, vnetConfig.status);
    if(env->ExceptionCheck() || statusObject == NULL)
    {
        return NULL;
    }
    env->SetObjectField(vnetConfigObj, VirtualNetworkConfig_status_field, statusObject);

    jobject typeObject = createVirtualNetworkType(env, vnetConfig.type);
    if(env->ExceptionCheck() || typeObject == NULL)
    {
        return NULL;
    }
    env->SetObjectField(vnetConfigObj, VirtualNetworkConfig_type_field, typeObject);

    env->SetIntField(vnetConfigObj, VirtualNetworkConfig_mtu_field, (int)vnetConfig.mtu);
    env->SetBooleanField(vnetConfigObj, VirtualNetworkConfig_dhcp_field, vnetConfig.dhcp);
    env->SetBooleanField(vnetConfigObj, VirtualNetworkConfig_bridge_field, vnetConfig.bridge);
    env->SetBooleanField(vnetConfigObj, VirtualNetworkConfig_broadcastEnabled_field, vnetConfig.broadcastEnabled);
    env->SetIntField(vnetConfigObj, VirtualNetworkConfig_portError_field, vnetConfig.portError);

    jobjectArray assignedAddrArrayObj = newInetSocketAddressArray(env, vnetConfig.assignedAddresses, vnetConfig.assignedAddressCount);

    env->SetObjectField(vnetConfigObj, VirtualNetworkConfig_assignedAddresses_field, assignedAddrArrayObj);

    jobjectArray routesArrayObj = newVirtualNetworkRouteArray(env, vnetConfig.routes, vnetConfig.routeCount);

    env->SetObjectField(vnetConfigObj, VirtualNetworkConfig_routes_field, routesArrayObj);

    jobject dnsObj = newVirtualNetworkDNS(env, vnetConfig.dns);
    if (dnsObj != NULL) {
        env->SetObjectField(vnetConfigObj, VirtualNetworkConfig_dns_field, dnsObj);
    }
    return vnetConfigObj;
}

jobject newVersion(JNIEnv *env, int major, int minor, int rev)
{
    // create a com.zerotier.sdk.Version object
    jobject versionObj = env->NewObject(Version_class, Version_ctor);
    if(env->ExceptionCheck() || versionObj == NULL)
    {
        return NULL;
    }

    env->SetIntField(versionObj, Version_major_field, (jint)major);
    env->SetIntField(versionObj, Version_minor_field, (jint)minor);
    env->SetIntField(versionObj, Version_revision_field, (jint)rev);

    return versionObj;
}

jobject newVirtualNetworkRoute(JNIEnv *env, const ZT_VirtualNetworkRoute &route)
{
    jobject routeObj = env->NewObject(VirtualNetworkRoute_class, VirtualNetworkRoute_ctor);
    if(env->ExceptionCheck() || routeObj == NULL)
    {
        return NULL;
    }

    jobject targetObj = newInetSocketAddress(env, route.target);
    jobject viaObj = newInetSocketAddress(env, route.via);

    env->SetObjectField(routeObj, VirtualNetworkRoute_target_field, targetObj);
    env->SetObjectField(routeObj, VirtualNetworkRoute_via_field, viaObj);
    env->SetIntField(routeObj, VirtualNetworkRoute_flags_field, (jint)route.flags);
    env->SetIntField(routeObj, VirtualNetworkRoute_metric_field, (jint)route.metric);

    return routeObj;
}

jobject newVirtualNetworkDNS(JNIEnv *env, const ZT_VirtualNetworkDNS &dns)
{
    jobject dnsObj = env->NewObject(VirtualNetworkDNS_class, VirtualNetworkDNS_ctor);
    if(env->ExceptionCheck() || dnsObj == NULL) {
        return NULL;
    }

    if (strlen(dns.domain) > 0) {

        jstring domain = env->NewStringUTF(dns.domain);

        jobject addrArray = env->NewObject(ArrayList_class, ArrayList_ctor, 0);

        struct sockaddr_storage nullAddr;
        memset(&nullAddr, 0, sizeof(struct sockaddr_storage));
        for(int i = 0; i < ZT_MAX_DNS_SERVERS; ++i) {
            struct sockaddr_storage tmp = dns.server_addr[i];

            if (memcmp(&tmp, &nullAddr, sizeof(struct sockaddr_storage)) != 0) {
                jobject addr = newInetSocketAddress(env, tmp);
                env->CallBooleanMethod(addrArray, ArrayList_add_method, addr);
                env->DeleteLocalRef(addr);
            }
        }

        env->SetObjectField(dnsObj, VirtualNetworkDNS_domain_field, domain);
        env->SetObjectField(dnsObj, VirtualNetworkDNS_servers_field, addrArray);

        return dnsObj;
    }
    return NULL;
}

jobject newNodeStatus(JNIEnv *env, const ZT_NodeStatus &status) {

    jstring pubIdentStr = env->NewStringUTF(status.publicIdentity);
    if(env->ExceptionCheck() || pubIdentStr == NULL)
    {
        LOGE("Exception creating new string");
        return NULL;
    }

    jstring secIdentStr = env->NewStringUTF(status.secretIdentity);
    if(env->ExceptionCheck() || secIdentStr == NULL)
    {
        LOGE("Exception creating new string");
        return NULL;
    }

    jobject nodeStatusObj = env->NewObject(
            NodeStatus_class,
            NodeStatus_ctor,
            status.address,
            pubIdentStr,
            secIdentStr,
            status.online);
    if(env->ExceptionCheck() || nodeStatusObj == NULL) {
        LOGE("Exception creating new NodeStatus");
        return NULL;
    }

    return nodeStatusObj;
}

jobjectArray newPeerArray(JNIEnv *env, const ZT_Peer *peers, size_t count) {
    return newArrayObject<ZT_Peer, newPeer>(env, peers, count, Peer_class);
}

jobjectArray newVirtualNetworkConfigArray(JNIEnv *env, const ZT_VirtualNetworkConfig *networks, size_t count) {
    return newArrayObject<ZT_VirtualNetworkConfig, newNetworkConfig>(env, networks, count, VirtualNetworkConfig_class);
}

jobjectArray newPeerPhysicalPathArray(JNIEnv *env, const ZT_PeerPhysicalPath *paths, size_t count) {
    return newArrayObject<ZT_PeerPhysicalPath, newPeerPhysicalPath>(env, paths, count, PeerPhysicalPath_class);
}

jobjectArray newInetSocketAddressArray(JNIEnv *env, const sockaddr_storage *addresses, size_t count) {
    return newArrayObject<sockaddr_storage, newInetSocketAddress>(env, addresses, count, InetSocketAddress_class);
}

jobjectArray newVirtualNetworkRouteArray(JNIEnv *env, const ZT_VirtualNetworkRoute *routes, size_t count) {
    return newArrayObject<ZT_VirtualNetworkRoute, newVirtualNetworkRoute>(env, routes, count, VirtualNetworkRoute_class);
}

void newArrayObject_logCount(size_t count) {
    LOGE("count > JSIZE_MAX: %zu", count);
}

void newArrayObject_log(const char *msg) {
    LOGE("%s", msg);
}

jbyteArray newByteArray(JNIEnv *env, const unsigned char *bytes, size_t count) {

    if (count > JSIZE_MAX) {
        LOGE("count > JSIZE_MAX: %zu", count);
        return NULL;
    }

    jsize jCount = static_cast<jsize>(count);
    const jbyte *jBytes = reinterpret_cast<const jbyte *>(bytes);

    jbyteArray byteArrayObj = env->NewByteArray(jCount);
    if(byteArrayObj == NULL)
    {
        LOGE("NewByteArray returned NULL");
        return NULL;
    }

    env->SetByteArrayRegion(byteArrayObj, 0, jCount, jBytes);
    if (env->ExceptionCheck()) {
        LOGE("Exception when calling SetByteArrayRegion");
        return NULL;
    }

    return byteArrayObj;
}

jbyteArray newByteArray(JNIEnv *env, size_t count) {

    if (count > JSIZE_MAX) {
        LOGE("count > JSIZE_MAX: %zu", count);
        return NULL;
    }

    jsize jCount = static_cast<jsize>(count);

    jbyteArray byteArrayObj = env->NewByteArray(jCount);
    if(byteArrayObj == NULL)
    {
        LOGE("NewByteArray returned NULL");
        return NULL;
    }

    return byteArrayObj;
}
