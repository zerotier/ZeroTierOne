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
    jobject resultObject = NULL;

    jfieldID field;
    switch(code)
    {
    case ZT_RESULT_OK:
    case ZT_RESULT_OK_IGNORED:
        LOGV("ZT_RESULT_OK");
        field = ResultCode_RESULT_OK_field;
        break;
    case ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY:
        LOGV("ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY");
        field = ResultCode_RESULT_FATAL_ERROR_OUT_OF_MEMORY_field;
        break;
    case ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED:
        LOGV("ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED");
        field = ResultCode_RESULT_FATAL_ERROR_DATA_STORE_FAILED_field;
        break;
    case ZT_RESULT_ERROR_NETWORK_NOT_FOUND:
        LOGV("ZT_RESULT_ERROR_NETWORK_NOT_FOUND");
        field = ResultCode_RESULT_ERROR_NETWORK_NOT_FOUND_field;
        break;
    case ZT_RESULT_ERROR_UNSUPPORTED_OPERATION:
        LOGV("ZT_RESULT_ERROR_UNSUPPORTED_OPERATION");
        field = ResultCode_RESULT_ERROR_UNSUPPORTED_OPERATION_field;
        break;
    case ZT_RESULT_ERROR_BAD_PARAMETER:
        LOGV("ZT_RESULT_ERROR_BAD_PARAMETER");
        field = ResultCode_RESULT_ERROR_BAD_PARAMETER_field;
        break;
    case ZT_RESULT_FATAL_ERROR_INTERNAL:
    default:
        LOGV("ZT_RESULT_FATAL_ERROR_INTERNAL");
        field = ResultCode_RESULT_FATAL_ERROR_INTERNAL_field;
        break;
    }

    resultObject = env->GetStaticObjectField(ResultCode_class, field);
    if(env->ExceptionCheck() || resultObject == NULL)
    {
        LOGE("Error on GetStaticObjectField");
    }
    return resultObject;
}


jobject createVirtualNetworkStatus(JNIEnv *env, ZT_VirtualNetworkStatus status)
{
    jobject statusObject = NULL;

    jfieldID field;
    switch(status)
    {
    case ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION:
        field = VirtualNetworkStatus_NETWORK_STATUS_REQUESTING_CONFIGURATION_field;
        break;
    case ZT_NETWORK_STATUS_OK:
        field = VirtualNetworkStatus_NETWORK_STATUS_OK_field;
        break;
    case ZT_NETWORK_STATUS_AUTHENTICATION_REQUIRED:
        field = VirtualNetworkStatus_NETWORK_STATUS_AUTHENTICATION_REQUIRED_field;
        break;
    case ZT_NETWORK_STATUS_ACCESS_DENIED:
        field = VirtualNetworkStatus_NETWORK_STATUS_ACCESS_DENIED_field;
        break;
    case ZT_NETWORK_STATUS_NOT_FOUND:
        field = VirtualNetworkStatus_NETWORK_STATUS_NOT_FOUND_field;
        break;
    case ZT_NETWORK_STATUS_PORT_ERROR:
        field = VirtualNetworkStatus_NETWORK_STATUS_PORT_ERROR_field;
        break;
    case ZT_NETWORK_STATUS_CLIENT_TOO_OLD:
        field = VirtualNetworkStatus_NETWORK_STATUS_CLIENT_TOO_OLD_field;
        break;
    }

    statusObject = env->GetStaticObjectField(VirtualNetworkStatus_class, field);

    return statusObject;
}

jobject createEvent(JNIEnv *env, ZT_Event event)
{
    jobject eventObject = NULL;

    jfieldID field;
    switch(event)
    {
    case ZT_EVENT_UP:
        field = Event_EVENT_UP_field;
        break;
    case ZT_EVENT_OFFLINE:
        field = Event_EVENT_OFFLINE_field;
        break;
    case ZT_EVENT_ONLINE:
        field = Event_EVENT_ONLINE_field;
        break;
    case ZT_EVENT_DOWN:
        field = Event_EVENT_DOWN_field;
        break;
    case ZT_EVENT_FATAL_ERROR_IDENTITY_COLLISION:
        field = Event_EVENT_FATAL_ERROR_IDENTITY_COLLISION_field;
        break;
    case ZT_EVENT_TRACE:
        field = Event_EVENT_TRACE_field;
        break;
    case ZT_EVENT_USER_MESSAGE:
        field = Event_EVENT_USER_MESSAGE_field;
        break;
    case ZT_EVENT_REMOTE_TRACE:
        field = Event_EVENT_REMOTE_TRACE_field;
        break;
    default:
        break;
    }

    eventObject = env->GetStaticObjectField(Event_class, field);

    return eventObject;
}

jobject createPeerRole(JNIEnv *env, ZT_PeerRole role)
{
    jobject peerRoleObject = NULL;

    jfieldID field;
    switch(role)
    {
    case ZT_PEER_ROLE_LEAF:
        field = PeerRole_PEER_ROLE_LEAF_field;
        break;
    case ZT_PEER_ROLE_MOON:
        field = PeerRole_PEER_ROLE_MOON_field;
        break;
    case ZT_PEER_ROLE_PLANET:
        field = PeerRole_PEER_ROLE_PLANET_field;
        break;
    }

    peerRoleObject = env->GetStaticObjectField(PeerRole_class, field);

    return peerRoleObject;
}

jobject createVirtualNetworkType(JNIEnv *env, ZT_VirtualNetworkType type)
{
    jobject vntypeObject = NULL;

    jfieldID field;
    switch(type)
    {
    case ZT_NETWORK_TYPE_PRIVATE:
        field = VirtualNetworkType_NETWORK_TYPE_PRIVATE_field;
        break;
    case ZT_NETWORK_TYPE_PUBLIC:
        field = VirtualNetworkType_NETWORK_TYPE_PUBLIC_field;
        break;
    }

    vntypeObject = env->GetStaticObjectField(VirtualNetworkType_class, field);
    return vntypeObject;
}

jobject createVirtualNetworkConfigOperation(JNIEnv *env, ZT_VirtualNetworkConfigOperation op)
{
    jobject vnetConfigOpObject = NULL;

    jfieldID field;
    switch(op)
    {
    case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP:
        field = VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_UP_field;
        break;
    case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE:
        field = VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE_field;
        break;
    case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN:
        field = VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN_field;
        break;
    case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY:
        field = VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY_field;
        break;
    }

    vnetConfigOpObject = env->GetStaticObjectField(VirtualNetworkConfigOperation_class, field);
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
            jbyteArray buff = env->NewByteArray(16);
            if(buff == NULL)
            {
                LOGE("Error creating IPV6 byte array");
                return NULL;
            }

            env->SetByteArrayRegion(buff, 0, 16, (jbyte*)ipv6->sin6_addr.s6_addr);
            inetAddressObj = env->CallStaticObjectMethod(
                InetAddress_class, InetAddress_getByAddress_method, buff);
        }
        break;
        case AF_INET:
        {
            sockaddr_in *ipv4 = (sockaddr_in*)&addr;
            jbyteArray buff = env->NewByteArray(4);
            if(buff == NULL)
            {
                LOGE("Error creating IPV4 byte array");
                return NULL;
            }

            env->SetByteArrayRegion(buff, 0, 4, (jbyte*)&ipv4->sin_addr);
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

    jobject pppObject = env->NewObject(PeerPhysicalPath_class, PeerPhysicalPath_ctor);
    if(env->ExceptionCheck() || pppObject == NULL)
    {
        LOGE("Error creating PPP object");
        return NULL; // out of memory
    }

    jobject addressObject = newInetSocketAddress(env, ppp.address);
    if(env->ExceptionCheck() || addressObject == NULL) {
        LOGE("Error creating InetSocketAddress object");
        return NULL;
    }

    env->SetObjectField(pppObject, PeerPhysicalPath_address_field, addressObject);
    env->SetLongField(pppObject, PeerPhysicalPath_lastSend_field, ppp.lastSend);
    env->SetLongField(pppObject, PeerPhysicalPath_lastReceive_field, ppp.lastReceive);
    env->SetBooleanField(pppObject, PeerPhysicalPath_preferred_field, ppp.preferred);

    if(env->ExceptionCheck()) {
        LOGE("Exception assigning fields to PeerPhysicalPath object");
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

    jobjectArray arrayObject = env->NewObjectArray(
        peer.pathCount, PeerPhysicalPath_class, NULL);
    if(env->ExceptionCheck() || arrayObject == NULL)
    {
        LOGE("Error creating PeerPhysicalPath[] array");
        return NULL;
    }

    for(unsigned int i = 0; i < peer.pathCount; ++i)
    {
        jobject path = newPeerPhysicalPath(env, peer.paths[i]);

        env->SetObjectArrayElement(arrayObject, i, path);
        if(env->ExceptionCheck()) {
            LOGE("exception assigning PeerPhysicalPath to array");
            break;
        }

        env->DeleteLocalRef(path);
    }

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

    jobjectArray assignedAddrArrayObj = env->NewObjectArray(
        vnetConfig.assignedAddressCount, InetSocketAddress_class, NULL);
    if(env->ExceptionCheck() || assignedAddrArrayObj == NULL)
    {
        LOGE("Error creating InetSocketAddress[] array");
        return NULL;
    }

    for(unsigned int i = 0; i < vnetConfig.assignedAddressCount; ++i)
    {
        jobject inetAddrObj = newInetSocketAddress(env, vnetConfig.assignedAddresses[i]);
        env->SetObjectArrayElement(assignedAddrArrayObj, i, inetAddrObj);
        if(env->ExceptionCheck())
        {
            LOGE("Error assigning InetSocketAddress to array");
            return NULL;
        }

        env->DeleteLocalRef(inetAddrObj);
    }

    env->SetObjectField(vnetConfigObj, VirtualNetworkConfig_assignedAddresses_field, assignedAddrArrayObj);

    jobjectArray routesArrayObj = env->NewObjectArray(
        vnetConfig.routeCount, VirtualNetworkRoute_class, NULL);
    if(env->ExceptionCheck() || routesArrayObj == NULL)
    {
        LOGE("Error creating VirtualNetworkRoute[] array");
        return NULL;
    }

    for(unsigned int i = 0; i < vnetConfig.routeCount; ++i)
    {
        jobject routeObj = newVirtualNetworkRoute(env, vnetConfig.routes[i]);
        env->SetObjectArrayElement(routesArrayObj, i, routeObj);
        if(env->ExceptionCheck())
        {
            LOGE("Error assigning VirtualNetworkRoute to array");
            return NULL;
        }

        env->DeleteLocalRef(routeObj);
    }

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
