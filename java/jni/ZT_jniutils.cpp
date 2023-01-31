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
#include "ZT_jnilookup.h"
#include "ZT_jniarray.h"

#include <string>
#include <cassert>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

extern JniLookup lookup;

jobject createResultObject(JNIEnv *env, ZT_ResultCode code)
{
    jclass resultClass = NULL;

    jobject resultObject = NULL;

    resultClass = lookup.findClass("com/zerotier/sdk/ResultCode");
    if(resultClass == NULL)
    {
        LOGE("Couldn't find ResultCode class");
        return NULL; // exception thrown
    }

    std::string fieldName;
    switch(code)
    {
    case ZT_RESULT_OK:
    case ZT_RESULT_OK_IGNORED:
        LOGV("ZT_RESULT_OK");
        fieldName = "RESULT_OK";
        break;
    case ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY:
        LOGV("ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY");
        fieldName = "RESULT_FATAL_ERROR_OUT_OF_MEMORY";
        break;
    case ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED:
        LOGV("ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED");
        fieldName = "RESULT_FATAL_ERROR_DATA_STORE_FAILED";
        break;
    case ZT_RESULT_ERROR_NETWORK_NOT_FOUND:
        LOGV("ZT_RESULT_ERROR_NETWORK_NOT_FOUND");
        fieldName = "RESULT_ERROR_NETWORK_NOT_FOUND";
        break;
    case ZT_RESULT_ERROR_UNSUPPORTED_OPERATION:
        LOGV("ZT_RESULT_ERROR_UNSUPPORTED_OPERATION");
        fieldName = "RESULT_ERROR_UNSUPPORTED_OPERATION";
        break;
    case ZT_RESULT_ERROR_BAD_PARAMETER:
        LOGV("ZT_RESULT_ERROR_BAD_PARAMETER");
        fieldName = "RESULT_ERROR_BAD_PARAMETER";
        break;
    case ZT_RESULT_FATAL_ERROR_INTERNAL:
    default:
        LOGV("ZT_RESULT_FATAL_ERROR_INTERNAL");
        fieldName = "RESULT_FATAL_ERROR_INTERNAL";
        break;
    }

    jfieldID enumField = lookup.findStaticField(resultClass, fieldName.c_str(), "Lcom/zerotier/sdk/ResultCode;");
    if(env->ExceptionCheck() || enumField == NULL)
    {
        LOGE("Error on FindStaticField");
        return NULL;
    }

    resultObject = env->GetStaticObjectField(resultClass, enumField);
    if(env->ExceptionCheck() || resultObject == NULL)
    {
        LOGE("Error on GetStaticObjectField");
    }
    return resultObject;
}


jobject createVirtualNetworkStatus(JNIEnv *env, ZT_VirtualNetworkStatus status)
{
    jobject statusObject = NULL;

    jclass statusClass = lookup.findClass("com/zerotier/sdk/VirtualNetworkStatus");
    if(statusClass == NULL)
    {
        return NULL; // exception thrown
    }

    std::string fieldName;
    switch(status)
    {
    case ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION:
        fieldName = "NETWORK_STATUS_REQUESTING_CONFIGURATION";
        break;
    case ZT_NETWORK_STATUS_OK:
        fieldName = "NETWORK_STATUS_OK";
        break;
    case ZT_NETWORK_STATUS_AUTHENTICATION_REQUIRED:
        fieldName = "NETWORK_STATUS_AUTHENTICATION_REQUIRED";
        break;
    case ZT_NETWORK_STATUS_ACCESS_DENIED:
        fieldName = "NETWORK_STATUS_ACCESS_DENIED";
        break;
    case ZT_NETWORK_STATUS_NOT_FOUND:
        fieldName = "NETWORK_STATUS_NOT_FOUND";
        break;
    case ZT_NETWORK_STATUS_PORT_ERROR:
        fieldName = "NETWORK_STATUS_PORT_ERROR";
        break;
    case ZT_NETWORK_STATUS_CLIENT_TOO_OLD:
        fieldName = "NETWORK_STATUS_CLIENT_TOO_OLD";
        break;
    }

    jfieldID enumField = lookup.findStaticField(statusClass, fieldName.c_str(), "Lcom/zerotier/sdk/VirtualNetworkStatus;");

    statusObject = env->GetStaticObjectField(statusClass, enumField);

    return statusObject;
}

jobject createEvent(JNIEnv *env, ZT_Event event)
{
    jclass eventClass = NULL;
    jobject eventObject = NULL;

    eventClass = lookup.findClass("com/zerotier/sdk/Event");
    if(eventClass == NULL)
    {
        return NULL;
    }

    std::string fieldName;
    switch(event)
    {
    case ZT_EVENT_UP:
        fieldName = "EVENT_UP";
        break;
    case ZT_EVENT_OFFLINE:
        fieldName = "EVENT_OFFLINE";
        break;
    case ZT_EVENT_ONLINE:
        fieldName = "EVENT_ONLINE";
        break;
    case ZT_EVENT_DOWN:
        fieldName = "EVENT_DOWN";
        break;
    case ZT_EVENT_FATAL_ERROR_IDENTITY_COLLISION:
        fieldName = "EVENT_FATAL_ERROR_IDENTITY_COLLISION";
        break;
    case ZT_EVENT_TRACE:
        fieldName = "EVENT_TRACE";
        break;
    case ZT_EVENT_USER_MESSAGE:
        break;
    case ZT_EVENT_REMOTE_TRACE:
    default:
        break;
    }

    jfieldID enumField = lookup.findStaticField(eventClass, fieldName.c_str(), "Lcom/zerotier/sdk/Event;");

    eventObject = env->GetStaticObjectField(eventClass, enumField);

    return eventObject;
}

jobject createPeerRole(JNIEnv *env, ZT_PeerRole role)
{
    jclass peerRoleClass = NULL;
    jobject peerRoleObject = NULL;

    peerRoleClass = lookup.findClass("com/zerotier/sdk/PeerRole");
    if(peerRoleClass == NULL)
    {
        return NULL;
    }

    std::string fieldName;
    switch(role)
    {
    case ZT_PEER_ROLE_LEAF:
        fieldName = "PEER_ROLE_LEAF";
        break;
    case ZT_PEER_ROLE_MOON:
        fieldName = "PEER_ROLE_MOON";
        break;
    case ZT_PEER_ROLE_PLANET:
        fieldName = "PEER_ROLE_PLANET";
        break;
    }

    jfieldID enumField = lookup.findStaticField(peerRoleClass, fieldName.c_str(), "Lcom/zerotier/sdk/PeerRole;");

    peerRoleObject = env->GetStaticObjectField(peerRoleClass, enumField);

    return peerRoleObject;
}

jobject createVirtualNetworkType(JNIEnv *env, ZT_VirtualNetworkType type)
{
    jclass vntypeClass = NULL;
    jobject vntypeObject = NULL;

    vntypeClass = lookup.findClass("com/zerotier/sdk/VirtualNetworkType");
    if(env->ExceptionCheck() || vntypeClass == NULL)
    {
        return NULL;
    }

    std::string fieldName;
    switch(type)
    {
    case ZT_NETWORK_TYPE_PRIVATE:
        fieldName = "NETWORK_TYPE_PRIVATE";
        break;
    case ZT_NETWORK_TYPE_PUBLIC:
        fieldName = "NETWORK_TYPE_PUBLIC";
        break;
    }

    jfieldID enumField = lookup.findStaticField(vntypeClass, fieldName.c_str(), "Lcom/zerotier/sdk/VirtualNetworkType;");
    vntypeObject = env->GetStaticObjectField(vntypeClass, enumField);
    return vntypeObject;
}

jobject createVirtualNetworkConfigOperation(JNIEnv *env, ZT_VirtualNetworkConfigOperation op)
{
    jclass vnetConfigOpClass = NULL;
    jobject vnetConfigOpObject = NULL;

    vnetConfigOpClass = lookup.findClass("com/zerotier/sdk/VirtualNetworkConfigOperation");
    if(env->ExceptionCheck() || vnetConfigOpClass == NULL)
    {
        return NULL;
    }

    std::string fieldName;
    switch(op)
    {
    case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP:
        fieldName = "VIRTUAL_NETWORK_CONFIG_OPERATION_UP";
        break;
    case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE:
        fieldName = "VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE";
        break;
    case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN:
        fieldName = "VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN";
        break;
    case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY:
        fieldName = "VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY";
        break;
    }

    jfieldID enumField = lookup.findStaticField(vnetConfigOpClass, fieldName.c_str(), "Lcom/zerotier/sdk/VirtualNetworkConfigOperation;");
    vnetConfigOpObject = env->GetStaticObjectField(vnetConfigOpClass, enumField);
    return vnetConfigOpObject;
}

jobject newInetAddress(JNIEnv *env, const sockaddr_storage &addr)
{
    LOGV("newInetAddress");
    jclass inetAddressClass = NULL;
    jmethodID inetAddress_getByAddress = NULL;

    inetAddressClass = lookup.findClass("java/net/InetAddress");
    if(env->ExceptionCheck() || inetAddressClass == NULL)
    {
        LOGE("Error finding InetAddress class");
        return NULL;
    }

    inetAddress_getByAddress = lookup.findStaticMethod(
        inetAddressClass, "getByAddress", "([B)Ljava/net/InetAddress;");
    if(env->ExceptionCheck() || inetAddress_getByAddress == NULL)
    {
        LOGE("Error finding getByAddress() static method");
        return NULL;
    }

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
                inetAddressClass, inetAddress_getByAddress, buff);
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
                inetAddressClass, inetAddress_getByAddress, buff);
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
    jclass inetSocketAddressClass = NULL;
    jmethodID inetSocketAddress_constructor = NULL;

    inetSocketAddressClass = lookup.findClass("java/net/InetSocketAddress");
    if(env->ExceptionCheck() || inetSocketAddressClass == NULL)
    {
        LOGE("Error finding InetSocketAddress Class");
        return NULL;
    }

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

    inetSocketAddress_constructor = lookup.findMethod(
        inetSocketAddressClass, "<init>", "(Ljava/net/InetAddress;I)V");
    if(env->ExceptionCheck() || inetSocketAddress_constructor == NULL)
    {
        LOGE("Error finding InetSocketAddress constructor");
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


    jobject inetSocketAddressObject = env->NewObject(inetSocketAddressClass, inetSocketAddress_constructor, inetAddressObject, port);
    if(env->ExceptionCheck() || inetSocketAddressObject == NULL) {
        LOGE("Error creating InetSocketAddress object");
    }
    return inetSocketAddressObject;
}

jobject newPeerPhysicalPath(JNIEnv *env, const ZT_PeerPhysicalPath &ppp)
{
    LOGV("newPeerPhysicalPath Called");
    jclass pppClass = NULL;

    jfieldID addressField = NULL;
    jfieldID lastSendField = NULL;
    jfieldID lastReceiveField = NULL;
    jfieldID preferredField = NULL;

    jmethodID ppp_constructor = NULL;

    pppClass = lookup.findClass("com/zerotier/sdk/PeerPhysicalPath");
    if(env->ExceptionCheck() || pppClass == NULL)
    {
        LOGE("Error finding PeerPhysicalPath class");
        return NULL;
    }

    addressField = lookup.findField(pppClass, "address", "Ljava/net/InetSocketAddress;");
    if(env->ExceptionCheck() || addressField == NULL)
    {
        LOGE("Error finding address field");
        return NULL;
    }

    lastSendField = lookup.findField(pppClass, "lastSend", "J");
    if(env->ExceptionCheck() || lastSendField == NULL)
    {
        LOGE("Error finding lastSend field");
        return NULL;
    }

    lastReceiveField = lookup.findField(pppClass, "lastReceive", "J");
    if(env->ExceptionCheck() || lastReceiveField == NULL)
    {
        LOGE("Error finding lastReceive field");
        return NULL;
    }

    preferredField = lookup.findField(pppClass, "preferred", "Z");
    if(env->ExceptionCheck() || preferredField == NULL)
    {
        LOGE("Error finding preferred field");
        return NULL;
    }

    ppp_constructor = lookup.findMethod(pppClass, "<init>", "()V");
    if(env->ExceptionCheck() || ppp_constructor == NULL)
    {
        LOGE("Error finding PeerPhysicalPath constructor");
        return NULL;
    }

    jobject pppObject = env->NewObject(pppClass, ppp_constructor);
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

    env->SetObjectField(pppObject, addressField, addressObject);
    env->SetLongField(pppObject, lastSendField, ppp.lastSend);
    env->SetLongField(pppObject, lastReceiveField, ppp.lastReceive);
    env->SetBooleanField(pppObject, preferredField, ppp.preferred);

    if(env->ExceptionCheck()) {
        LOGE("Exception assigning fields to PeerPhysicalPath object");
    }

    return pppObject;
}

jobject newPeer(JNIEnv *env, const ZT_Peer &peer)
{
    LOGV("newPeer called");

    jclass peerClass = NULL;

    jfieldID addressField = NULL;
    jfieldID versionMajorField = NULL;
    jfieldID versionMinorField = NULL;
    jfieldID versionRevField = NULL;
    jfieldID latencyField = NULL;
    jfieldID roleField = NULL;
    jfieldID pathsField = NULL;

    jmethodID peer_constructor = NULL;

    peerClass = lookup.findClass("com/zerotier/sdk/Peer");
    if(env->ExceptionCheck() || peerClass == NULL)
    {
        LOGE("Error finding Peer class");
        return NULL;
    }

    addressField = lookup.findField(peerClass, "address", "J");
    if(env->ExceptionCheck() || addressField == NULL)
    {
        LOGE("Error finding address field of Peer object");
        return NULL;
    }

    versionMajorField = lookup.findField(peerClass, "versionMajor", "I");
    if(env->ExceptionCheck() || versionMajorField == NULL)
    {
        LOGE("Error finding versionMajor field of Peer object");
        return NULL;
    }

    versionMinorField = lookup.findField(peerClass, "versionMinor", "I");
    if(env->ExceptionCheck() || versionMinorField == NULL)
    {
        LOGE("Error finding versionMinor field of Peer object");
        return NULL;
    }

    versionRevField = lookup.findField(peerClass, "versionRev", "I");
    if(env->ExceptionCheck() || versionRevField == NULL)
    {
        LOGE("Error finding versionRev field of Peer object");
        return NULL;
    }

    latencyField = lookup.findField(peerClass, "latency", "I");
    if(env->ExceptionCheck() || latencyField == NULL)
    {
        LOGE("Error finding latency field of Peer object");
        return NULL;
    }

    roleField = lookup.findField(peerClass, "role", "Lcom/zerotier/sdk/PeerRole;");
    if(env->ExceptionCheck() || roleField == NULL)
    {
        LOGE("Error finding role field of Peer object");
        return NULL;
    }

    pathsField = lookup.findField(peerClass, "paths", "[Lcom/zerotier/sdk/PeerPhysicalPath;");
    if(env->ExceptionCheck() || pathsField == NULL)
    {
        LOGE("Error finding paths field of Peer object");
        return NULL;
    }

    peer_constructor = lookup.findMethod(peerClass, "<init>", "()V");
    if(env->ExceptionCheck() || peer_constructor == NULL)
    {
        LOGE("Error finding Peer constructor");
        return NULL;
    }

    jobject peerObject = env->NewObject(peerClass, peer_constructor);
    if(env->ExceptionCheck() || peerObject == NULL)
    {
        LOGE("Error creating Peer object");
        return NULL; // out of memory
    }

    env->SetLongField(peerObject, addressField, (jlong)peer.address);
    env->SetIntField(peerObject, versionMajorField, peer.versionMajor);
    env->SetIntField(peerObject, versionMinorField, peer.versionMinor);
    env->SetIntField(peerObject, versionRevField, peer.versionRev);
    env->SetIntField(peerObject, latencyField, peer.latency);
    env->SetObjectField(peerObject, roleField, createPeerRole(env, peer.role));

    jclass peerPhysicalPathClass = lookup.findClass("com/zerotier/sdk/PeerPhysicalPath");
    if(env->ExceptionCheck() || peerPhysicalPathClass == NULL)
    {
        LOGE("Error finding PeerPhysicalPath class");
        return NULL;
    }

    jobjectArray arrayObject = env->NewObjectArray(
        peer.pathCount, peerPhysicalPathClass, NULL);
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
    }

    env->SetObjectField(peerObject, pathsField, arrayObject);

    return peerObject;
}

jobject newNetworkConfig(JNIEnv *env, const ZT_VirtualNetworkConfig &vnetConfig)
{
    jclass vnetConfigClass = NULL;
    jmethodID vnetConfig_constructor = NULL;
    jfieldID nwidField = NULL;
    jfieldID macField = NULL;
    jfieldID nameField = NULL;
    jfieldID statusField = NULL;
    jfieldID typeField = NULL;
    jfieldID mtuField = NULL;
    jfieldID dhcpField = NULL;
    jfieldID bridgeField = NULL;
    jfieldID broadcastEnabledField = NULL;
    jfieldID portErrorField = NULL;
    jfieldID netconfRevisionField = NULL;
    jfieldID assignedAddressesField = NULL;
    jfieldID routesField = NULL;
    jfieldID dnsField = NULL;

    vnetConfigClass = lookup.findClass("com/zerotier/sdk/VirtualNetworkConfig");
    if(vnetConfigClass == NULL)
    {
        LOGE("Couldn't find com.zerotier.sdk.VirtualNetworkConfig");
        return NULL;
    }

    vnetConfig_constructor = lookup.findMethod(
        vnetConfigClass, "<init>", "()V");
    if(env->ExceptionCheck() || vnetConfig_constructor == NULL)
    {
        LOGE("Couldn't find VirtualNetworkConfig Constructor");
        return NULL;
    }

    jobject vnetConfigObj = env->NewObject(vnetConfigClass, vnetConfig_constructor);
    if(env->ExceptionCheck() || vnetConfigObj == NULL)
    {
        LOGE("Error creating new VirtualNetworkConfig object");
        return NULL;
    }

    nwidField = lookup.findField(vnetConfigClass, "nwid", "J");
    if(env->ExceptionCheck() || nwidField == NULL)
    {
        LOGE("Error getting nwid field");
        return NULL;
    }

    macField = lookup.findField(vnetConfigClass, "mac", "J");
    if(env->ExceptionCheck() || macField == NULL)
    {
        LOGE("Error getting mac field");
        return NULL;
    }

    nameField = lookup.findField(vnetConfigClass, "name", "Ljava/lang/String;");
    if(env->ExceptionCheck() || nameField == NULL)
    {
        LOGE("Error getting name field");
        return NULL;
    }

    statusField = lookup.findField(vnetConfigClass, "status", "Lcom/zerotier/sdk/VirtualNetworkStatus;");
    if(env->ExceptionCheck() || statusField == NULL)
    {
        LOGE("Error getting status field");
        return NULL;
    }

    typeField = lookup.findField(vnetConfigClass, "type", "Lcom/zerotier/sdk/VirtualNetworkType;");
    if(env->ExceptionCheck() || typeField == NULL)
    {
        LOGE("Error getting type field");
        return NULL;
    }

    mtuField = lookup.findField(vnetConfigClass, "mtu", "I");
    if(env->ExceptionCheck() || mtuField == NULL)
    {
        LOGE("Error getting mtu field");
        return NULL;
    }

    dhcpField = lookup.findField(vnetConfigClass, "dhcp", "Z");
    if(env->ExceptionCheck() || dhcpField == NULL)
    {
        LOGE("Error getting dhcp field");
        return NULL;
    }

    bridgeField = lookup.findField(vnetConfigClass, "bridge", "Z");
    if(env->ExceptionCheck() || bridgeField == NULL)
    {
        LOGE("Error getting bridge field");
        return NULL;
    }

    broadcastEnabledField = lookup.findField(vnetConfigClass, "broadcastEnabled", "Z");
    if(env->ExceptionCheck() || broadcastEnabledField == NULL)
    {
        LOGE("Error getting broadcastEnabled field");
        return NULL;
    }

    portErrorField = lookup.findField(vnetConfigClass, "portError", "I");
    if(env->ExceptionCheck() || portErrorField == NULL)
    {
        LOGE("Error getting portError field");
        return NULL;
    }

    netconfRevisionField = lookup.findField(vnetConfigClass, "netconfRevision", "J");
    if(env->ExceptionCheck() || netconfRevisionField == NULL)
    {
        LOGE("Error getting netconfRevision field");
        return NULL;
    }

    assignedAddressesField = lookup.findField(vnetConfigClass, "assignedAddresses",
        "[Ljava/net/InetSocketAddress;");
    if(env->ExceptionCheck() || assignedAddressesField == NULL)
    {
        LOGE("Error getting assignedAddresses field");
        return NULL;
    }

    routesField = lookup.findField(vnetConfigClass, "routes",
        "[Lcom/zerotier/sdk/VirtualNetworkRoute;");
    if(env->ExceptionCheck() || routesField == NULL)
    {
        LOGE("Error getting routes field");
        return NULL;
    }

    dnsField = lookup.findField(vnetConfigClass, "dns", "Lcom/zerotier/sdk/VirtualNetworkDNS;");
    if(env->ExceptionCheck() || dnsField == NULL)
    {
        LOGE("Error getting DNS field");
        return NULL;
    }

    env->SetLongField(vnetConfigObj, nwidField, vnetConfig.nwid);
    env->SetLongField(vnetConfigObj, macField, vnetConfig.mac);
    jstring nameStr = env->NewStringUTF(vnetConfig.name);
    if(env->ExceptionCheck() || nameStr == NULL)
    {
        return NULL; // out of memory
    }
    env->SetObjectField(vnetConfigObj, nameField, nameStr);

    jobject statusObject = createVirtualNetworkStatus(env, vnetConfig.status);
    if(env->ExceptionCheck() || statusObject == NULL)
    {
        return NULL;
    }
    env->SetObjectField(vnetConfigObj, statusField, statusObject);

    jobject typeObject = createVirtualNetworkType(env, vnetConfig.type);
    if(env->ExceptionCheck() || typeObject == NULL)
    {
        return NULL;
    }
    env->SetObjectField(vnetConfigObj, typeField, typeObject);

    env->SetIntField(vnetConfigObj, mtuField, (int)vnetConfig.mtu);
    env->SetBooleanField(vnetConfigObj, dhcpField, vnetConfig.dhcp);
    env->SetBooleanField(vnetConfigObj, bridgeField, vnetConfig.bridge);
    env->SetBooleanField(vnetConfigObj, broadcastEnabledField, vnetConfig.broadcastEnabled);
    env->SetIntField(vnetConfigObj, portErrorField, vnetConfig.portError);

    jclass inetSocketAddressClass = lookup.findClass("java/net/InetSocketAddress");
    if(env->ExceptionCheck() || inetSocketAddressClass == NULL)
    {
        LOGE("Error finding InetSocketAddress class");
        return NULL;
    }

    jobjectArray assignedAddrArrayObj = env->NewObjectArray(
        vnetConfig.assignedAddressCount, inetSocketAddressClass, NULL);
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
    }

    env->SetObjectField(vnetConfigObj, assignedAddressesField, assignedAddrArrayObj);

    jclass virtualNetworkRouteClass = lookup.findClass("com/zerotier/sdk/VirtualNetworkRoute");
    if(env->ExceptionCheck() || virtualNetworkRouteClass == NULL)
    {
        LOGE("Error finding VirtualNetworkRoute class");
        return NULL;
    }

    jobjectArray routesArrayObj = env->NewObjectArray(
        vnetConfig.routeCount, virtualNetworkRouteClass, NULL);
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
    }

    env->SetObjectField(vnetConfigObj, routesField, routesArrayObj);

    jobject dnsObj = newVirtualNetworkDNS(env, vnetConfig.dns);
    if (dnsObj != NULL) {
        env->SetObjectField(vnetConfigObj, dnsField, dnsObj);
    }
    return vnetConfigObj;
}

jobject newVersion(JNIEnv *env, int major, int minor, int rev)
{
   // create a com.zerotier.sdk.Version object
    jclass versionClass = NULL;
    jmethodID versionConstructor = NULL;

    versionClass = lookup.findClass("com/zerotier/sdk/Version");
    if(env->ExceptionCheck() || versionClass == NULL)
    {
        return NULL;
    }

    versionConstructor = lookup.findMethod(
        versionClass, "<init>", "()V");
    if(env->ExceptionCheck() || versionConstructor == NULL)
    {
        return NULL;
    }

    jobject versionObj = env->NewObject(versionClass, versionConstructor);
    if(env->ExceptionCheck() || versionObj == NULL)
    {
        return NULL;
    }

    // copy data to Version object
    jfieldID majorField = NULL;
    jfieldID minorField = NULL;
    jfieldID revisionField = NULL;

    majorField = lookup.findField(versionClass, "major", "I");
    if(env->ExceptionCheck() || majorField == NULL)
    {
        return NULL;
    }

    minorField = lookup.findField(versionClass, "minor", "I");
    if(env->ExceptionCheck() || minorField == NULL)
    {
        return NULL;
    }

    revisionField = lookup.findField(versionClass, "revision", "I");
    if(env->ExceptionCheck() || revisionField == NULL)
    {
        return NULL;
    }

    env->SetIntField(versionObj, majorField, (jint)major);
    env->SetIntField(versionObj, minorField, (jint)minor);
    env->SetIntField(versionObj, revisionField, (jint)rev);

    return versionObj;
}

jobject newVirtualNetworkRoute(JNIEnv *env, const ZT_VirtualNetworkRoute &route)
{
    jclass virtualNetworkRouteClass = NULL;
    jmethodID routeConstructor = NULL;

    virtualNetworkRouteClass = lookup.findClass("com/zerotier/sdk/VirtualNetworkRoute");
    if(env->ExceptionCheck() || virtualNetworkRouteClass == NULL)
    {
        return NULL;
    }

    routeConstructor = lookup.findMethod(virtualNetworkRouteClass, "<init>", "()V");
    if(env->ExceptionCheck() || routeConstructor == NULL)
    {
        return NULL;
    }

    jobject routeObj = env->NewObject(virtualNetworkRouteClass, routeConstructor);
    if(env->ExceptionCheck() || routeObj == NULL)
    {
        return NULL;
    }

    jfieldID targetField = NULL;
    jfieldID viaField = NULL;
    jfieldID flagsField = NULL;
    jfieldID metricField = NULL;

    targetField = lookup.findField(virtualNetworkRouteClass, "target",
        "Ljava/net/InetSocketAddress;");
    if(env->ExceptionCheck() || targetField == NULL)
    {
        return NULL;
    }

    viaField = lookup.findField(virtualNetworkRouteClass, "via",
        "Ljava/net/InetSocketAddress;");
    if(env->ExceptionCheck() || targetField == NULL)
    {
        return NULL;
    }

    flagsField = lookup.findField(virtualNetworkRouteClass, "flags", "I");
    if(env->ExceptionCheck() || flagsField == NULL)
    {
        return NULL;
    }

    metricField = lookup.findField(virtualNetworkRouteClass, "metric", "I");
    if(env->ExceptionCheck() || metricField == NULL)
    {
        return NULL;
    }

    jobject targetObj = newInetSocketAddress(env, route.target);
    jobject viaObj = newInetSocketAddress(env, route.via);

    env->SetObjectField(routeObj, targetField, targetObj);
    env->SetObjectField(routeObj, viaField, viaObj);
    env->SetIntField(routeObj, flagsField, (jint)route.flags);
    env->SetIntField(routeObj, metricField, (jint)route.metric);

    return routeObj;
}

jobject newVirtualNetworkDNS(JNIEnv *env, const ZT_VirtualNetworkDNS &dns)
{
    jclass virtualNetworkDNSClass = NULL;
    jmethodID dnsConstructor = NULL;

    virtualNetworkDNSClass = lookup.findClass("com/zerotier/sdk/VirtualNetworkDNS");
    if (env->ExceptionCheck() || virtualNetworkDNSClass == NULL) {
        return NULL;
    }

    dnsConstructor = lookup.findMethod(virtualNetworkDNSClass, "<init>", "()V");
    if(env->ExceptionCheck() || dnsConstructor == NULL) {
        return NULL;
    }

    jobject dnsObj = env->NewObject(virtualNetworkDNSClass, dnsConstructor);
    if(env->ExceptionCheck() || dnsObj == NULL) {
        return NULL;
    }

    jfieldID domainField = NULL;
    jfieldID serversField = NULL;

    domainField = lookup.findField(virtualNetworkDNSClass, "domain", "Ljava/lang/String;");
    if(env->ExceptionCheck() || domainField == NULL)
    {
        return NULL;
    }

    serversField = lookup.findField(virtualNetworkDNSClass, "servers", "Ljava/util/ArrayList;");
    if(env->ExceptionCheck() || serversField == NULL) {
        return NULL;
    }

    if (strlen(dns.domain) > 0) {
        InitListJNI(env);
        jstring domain = env->NewStringUTF(dns.domain);

        jobject addrArray = env->NewObject(java_util_ArrayList, java_util_ArrayList_, 0);

        struct sockaddr_storage nullAddr;
        memset(&nullAddr, 0, sizeof(struct sockaddr_storage));
        for(int i = 0; i < ZT_MAX_DNS_SERVERS; ++i) {
            struct sockaddr_storage tmp = dns.server_addr[i];

            if (memcmp(&tmp, &nullAddr, sizeof(struct sockaddr_storage)) != 0) {
                jobject addr = newInetSocketAddress(env, tmp);
                env->CallBooleanMethod(addrArray, java_util_ArrayList_add, addr);
                env->DeleteLocalRef(addr);
            }
        }

        env->SetObjectField(dnsObj, domainField, domain);
        env->SetObjectField(dnsObj, serversField, addrArray);

        return dnsObj;
    }
    return NULL;
}
