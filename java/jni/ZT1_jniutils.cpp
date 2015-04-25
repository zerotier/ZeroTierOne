#include "ZT1_jniutils.h"
#include <string>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

namespace
{
    static jclass arrayListClass = NULL;
    static jmethodID arrayList_constructor = NULL;
    static jmethodID arrayList_add = NULL;

    static jclass inetAddressClass = NULL;
    static jmethodID  inetAddress_getByAddress = NULL;
}

jobject createResultObject(JNIEnv *env, ZT1_ResultCode code)
{
    // cache the class so we don't have to
    // look it up every time we need to create a java
    // ResultCode object
    static jclass resultClass = NULL;
    
    jobject resultObject = NULL;

    if(resultClass == NULL)
    {
        resultClass = env->FindClass("com/zerotierone/sdk/ResultCode");
        if(resultClass == NULL)
        {
            return NULL; // exception thrown
        }
    }

    std::string fieldName;
    switch(code)
    {
    case ZT1_RESULT_OK:
        fieldName = "ZT1_RESULT_OK";
        break;
    case ZT1_RESULT_FATAL_ERROR_OUT_OF_MEMORY:
        fieldName = "ZT1_RESULT_FATAL_ERROR_OUT_OF_MEMORY";
        break;
    case ZT1_RESULT_FATAL_ERROR_DATA_STORE_FAILED:
        fieldName = "ZT1_RESULT_FATAL_ERROR_DATA_STORE_FAILED";
        break;
    case ZT1_RESULT_ERROR_NETWORK_NOT_FOUND:
        fieldName = "ZT1_RESULT_ERROR_NETWORK_NOT_FOUND";
        break;
    case ZT1_RESULT_FATAL_ERROR_INTERNAL:
    default:
        fieldName = "ZT1_RESULT_FATAL_ERROR_INTERNAL";
        break;
    }

    jfieldID enumField = env->GetStaticFieldID(resultClass, fieldName.c_str(), "Lcom/zerotierone/sdk/ResultCode;");

    resultObject = env->GetStaticObjectField(resultClass, enumField);

    return resultObject;
}


jobject createVirtualNetworkStatus(JNIEnv *env, ZT1_VirtualNetworkStatus status)
{
    static jclass statusClass = NULL;
    
    jobject statusObject = NULL;

    if(statusClass == NULL)
    {
        statusClass = env->FindClass("com/zerotierone/sdk/VirtualNetworkStatus");
        if(statusClass == NULL)
        {
            return NULL; // exception thrown
        }
    }

    std::string fieldName;
    switch(status)
    {
    case ZT1_NETWORK_STATUS_REQUESTING_CONFIGURATION:
        fieldName = "NETWORK_STATUS_REQUESTING_CONFIGURATION";
        break;
    case ZT1_NETWORK_STATUS_OK:
        fieldName = "NETWORK_STATUS_OK";
        break;
    case ZT1_NETWORK_STATUS_ACCESS_DENIED:
        fieldName = "NETWORK_STATUS_ACCESS_DENIED";
        break;
    case ZT1_NETWORK_STATUS_NOT_FOUND:
        fieldName = "NETWORK_STATUS_NOT_FOUND";
        break;
    case ZT1_NETWORK_STATUS_PORT_ERROR:
        fieldName = "NETWORK_STATUS_PORT_ERROR";
        break;
    case ZT1_NETWORK_STATUS_CLIENT_TOO_OLD:
        fieldName = "NETWORK_STATUS_CLIENT_TOO_OLD";
        break;
    }

    jfieldID enumField = env->GetStaticFieldID(statusClass, fieldName.c_str(), "Lcom/zerotierone/sdk/VirtualNetworkStatus;");

    statusObject = env->GetStaticObjectField(statusClass, enumField);

    return statusObject;
}

jobject createEvent(JNIEnv *env, ZT1_Event event)
{
    static jclass eventClass = NULL;
    jobject eventObject = NULL;

    if(eventClass == NULL)
    {
        eventClass = env->FindClass("com/zerotierone/sdk/Event");
        if(eventClass == NULL)
        {
            return NULL;
        }
    }

    std::string fieldName;
    switch(event)
    {
    case ZT1_EVENT_UP:
        fieldName = "EVENT_UP";
        break;
    case ZT1_EVENT_OFFLINE:
        fieldName = "EVENT_OFFLINE";
        break;
    case ZT1_EVENT_DOWN:
        fieldName = "EVENT_DOWN";
        break;
    case ZT1_EVENT_FATAL_ERROR_IDENTITY_COLLISION:
        fieldName = "EVENT_FATAL_ERROR_IDENTITY_COLLISION";
        break;
    case ZT1_EVENT_AUTHENTICATION_FAILURE:
        fieldName = "EVENT_AUTHENTICATION_FAILURE";
        break;
    case ZT1_EVENT_INVALID_PACKET:
        fieldName = "EVENT_INVALID_PACKET";
        break;
    case ZT1_EVENT_TRACE:
        fieldName = "EVENT_TRACE";
        break;
    }

    jfieldID enumField = env->GetStaticFieldID(eventClass, fieldName.c_str(), "Lcom/zerotierone/sdk/Event;");

    eventObject = env->GetStaticObjectField(eventClass, enumField);

    return eventObject;
}

jobject createPeerRole(JNIEnv *env, ZT1_PeerRole role)
{
    static jclass peerRoleClass = NULL;
    jobject peerRoleObject = NULL;

    if(peerRoleClass == NULL)
    {
        peerRoleClass = env->FindClass("com/zerotierone/sdk/PeerRole");
        if(peerRoleClass == NULL)
        {
            return NULL;
        }
    }

    std::string fieldName;
    switch(role)
    {
    case ZT1_PEER_ROLE_LEAF:
        fieldName = "PEER_ROLE_LEAF";
        break;
    case ZT1_PEER_ROLE_HUB:
        fieldName = "PEER_ROLE_HUB";
        break;
    case ZT1_PEER_ROLE_SUPERNODE:
        fieldName = "PEER_ROLE_SUPERNODE";
        break;
    }

    jfieldID enumField = env->GetStaticFieldID(peerRoleClass, fieldName.c_str(), "Lcom/zerotierone/sdk/PeerRole;");

    peerRoleObject = env->GetStaticObjectField(peerRoleClass, enumField);

    return peerRoleObject;
}

jobject createVirtualNetworkType(JNIEnv *env, ZT1_VirtualNetworkType type)
{
    static jclass vntypeClass = NULL;
    jobject vntypeObject = NULL;

    if(vntypeClass == NULL)
    {
        vntypeClass = env->FindClass("com/zerotierone/sdk/VirtualNetworkType");
        if(vntypeClass == NULL)
        {
            return NULL;
        }
    }

    std::string fieldName;
    switch(type)
    {
    case ZT1_NETWORK_TYPE_PRIVATE:
        fieldName = "NETWORK_TYPE_PRIVATE";
        break;
    case ZT1_NETWORK_TYPE_PUBLIC:
        fieldName = "NETWORK_TYPE_PUBLIC";
        break;
    }

    jfieldID enumField = env->GetStaticFieldID(vntypeClass, fieldName.c_str(), "Lcom/zerotierone/sdk/VirtyalNetworkType;");
    vntypeObject = env->GetStaticObjectField(vntypeClass, enumField);
    return vntypeObject;
}

jobject newArrayList(JNIEnv *env)
{
    if(arrayListClass == NULL)
    {
        arrayListClass = env->FindClass("java/util/ArrayList");
        if(arrayListClass == NULL)
        {
            return NULL;
        }
    }

    if(arrayList_constructor == NULL)
    {
        arrayList_constructor = env->GetMethodID(
            arrayListClass, "<init>", "()V");
        if(arrayList_constructor == NULL)
        {
            return NULL;
        }
    }

    jobject arrayListObj = env->NewObject(arrayListClass, arrayList_constructor);

    return arrayListObj;
}

bool appendItemToArrayList(JNIEnv *env, jobject array, jobject object)
{
    assert(array != NULL);
    assert(object != NULL);

    if(arrayList_add == NULL)
    {
        arrayList_add = env->GetMethodID(arrayListClass, "add", "(Ljava.lang.Object;)Z");
        if(arrayList_add == NULL)
        {
            return false;
        }
    }

    return env->CallBooleanMethod(array, arrayList_add, object);
}

jobject newInetAddress(JNIEnv *env, const sockaddr_storage &addr)
{
    if(inetAddressClass == NULL)
    {
        inetAddressClass = env->FindClass("java/net/InetAddress");
        if(inetAddressClass == NULL)
        {
            return NULL;
        }
    }

    if(inetAddress_getByAddress == NULL)
    {
        inetAddress_getByAddress = env->GetStaticMethodID(
            inetAddressClass, "getByAddress", "([B)Ljava/net/InetAddress;");
        if(inetAddress_getByAddress == NULL)
        {
            return NULL;
        }
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
                return NULL;
            }

            env->SetByteArrayRegion(buff, 0, 4, (jbyte*)&ipv4->sin_addr);
            inetAddressObj = env->CallStaticObjectMethod(
                inetAddressClass, inetAddress_getByAddress, buff);
        }
        break;
    }

    return inetAddressObj;
}

jobject newMulticastGroup(JNIEnv *env, const ZT1_MulticastGroup &mc)
{
    static jclass multicastGroupClass = NULL;
    static jmethodID multicastGroup_constructor = NULL;

    static jfieldID macField = NULL;
    static jfieldID adiField = NULL;

    if(multicastGroupClass == NULL)
    {
        multicastGroupClass = env->FindClass("com/zerotierone/sdk/MulticastGroup");
        if(multicastGroupClass == NULL)
        {
            return NULL;
        }
    }

    if(multicastGroup_constructor == NULL)
    {
        multicastGroup_constructor = env->GetMethodID(
            multicastGroupClass, "<init>", "()V");
        if(multicastGroup_constructor == NULL)
        {
            return NULL;
        }
    }

    jobject multicastGroupObj = env->NewObject(multicastGroupClass, multicastGroup_constructor);
    if(multicastGroupObj == NULL)
    {
        return NULL;
    }

    if(macField == NULL)
    {
        macField = env->GetFieldID(multicastGroupClass, "mac", "J");
        if(macField == NULL)
        {
            return NULL;
        }
    }

    if(adiField == NULL)
    {
        adiField = env->GetFieldID(multicastGroupClass, "adi", "J");
        if(adiField == NULL)
        {
            return NULL;
        }
    }

    env->SetLongField(multicastGroupObj, macField, mc.mac);
    env->SetLongField(multicastGroupObj, adiField, mc.adi);

    return multicastGroupObj;
}

jobject newPeerPhysicalPath(JNIEnv *env, const ZT1_PeerPhysicalPath &ppp)
{
    static jclass pppClass = NULL;

    static jfieldID addressField = NULL;
    static jfieldID lastSendField = NULL;
    static jfieldID lastReceiveField = NULL;
    static jfieldID fixedField = NULL;
    static jfieldID activeField = NULL;
    static jfieldID preferredField = NULL;

    static jmethodID ppp_constructor = NULL;

    if(pppClass == NULL)
    {
        pppClass = env->FindClass("com/zerotierone/sdk/PeerPhysicalPath");
        if(pppClass == NULL)
        {
            return NULL;
        }
    }

    if(addressField == NULL)
    {
        addressField = env->GetFieldID(pppClass, "address", "Ljava/net/InetAddress;");
        if(addressField == NULL)
        {
            return NULL;
        }
    }

    if(lastSendField == NULL)
    {
        lastSendField = env->GetFieldID(pppClass, "lastSend", "J");
        if(lastSendField == NULL)
        {
            return NULL;
        }
    }

    if(lastReceiveField == NULL)
    {
        lastReceiveField = env->GetFieldID(pppClass, "lastReceive", "J");
        if(lastReceiveField == NULL)
        {
            return NULL;
        }
    }

    if(fixedField == NULL)
    {
        fixedField = env->GetFieldID(pppClass, "fixed", "Z");
        if(fixedField == NULL)
        {
            return NULL;
        }
    }

    if(activeField == NULL)
    {
        activeField = env->GetFieldID(pppClass, "active", "Z");
        if(activeField == NULL)
        {
            return NULL;
        }
    }

    if(preferredField == NULL)
    {
        preferredField = env->GetFieldID(pppClass, "preferred", "Z");
        if(preferredField == NULL)
        {
            return NULL;
        }
    }

    if(ppp_constructor == NULL)
    {
        ppp_constructor = env->GetMethodID(pppClass, "<init>", "()V");
        if(ppp_constructor == NULL)
        {
            return NULL;
        }
    }

    jobject pppObject = env->NewObject(pppClass, ppp_constructor);
    if(pppObject == NULL)
    {
        return NULL; // out of memory
    }

    jobject addressObject = newInetAddress(env, ppp.address);

    env->SetObjectField(pppClass, addressField, addressObject);
    env->SetLongField(pppClass, lastSendField, ppp.lastSend);
    env->SetLongField(pppClass, lastReceiveField, ppp.lastReceive);
    env->SetBooleanField(pppClass, fixedField, ppp.fixed);
    env->SetBooleanField(pppClass, activeField, ppp.active);
    env->SetBooleanField(pppClass, preferredField, ppp.preferred);

    return pppObject;
}

jobject newPeer(JNIEnv *env, const ZT1_Peer &peer) 
{
    static jclass peerClass = NULL;

    static jfieldID addressField = NULL;
    static jfieldID lastUnicastFrameField = NULL;
    static jfieldID lastMulticastFrameField = NULL;
    static jfieldID versionMajorField = NULL;
    static jfieldID versionMinorField = NULL;
    static jfieldID versionRevField = NULL;
    static jfieldID latencyField = NULL;
    static jfieldID roleField = NULL;
    static jfieldID pathsField = NULL;

    static jmethodID peer_constructor = NULL;

    if(peerClass == NULL)
    {
        peerClass = env->FindClass("com/zerotierone/sdk/Peer");
        if(peerClass == NULL)
        {
            return NULL;
        }
    }

    if(addressField == NULL)
    {
        addressField = env->GetFieldID(peerClass, "address", "J");
        if(addressField == NULL)
        {
            return NULL;
        }
    }

    if(lastUnicastFrameField == NULL)
    {
        lastUnicastFrameField = env->GetFieldID(peerClass, "lastUnicastFrame", "J");
        if(lastUnicastFrameField == NULL)
        {
            return NULL;
        }
    }

    if(lastMulticastFrameField == NULL)
    {
        lastMulticastFrameField = env->GetFieldID(peerClass, "lastMulticastFrame", "J");
        if(lastMulticastFrameField == NULL)
        {
            return NULL;
        }
    }

    if(versionMajorField == NULL)
    {
        versionMajorField = env->GetFieldID(peerClass, "versionMajor", "I");
        if(versionMajorField == NULL)
        {
            return NULL;
        }
    }

    if(versionMinorField == NULL)
    {
        versionMinorField = env->GetFieldID(peerClass, "versionMinor", "I");
        if(versionMinorField == NULL)
        {
            return NULL;
        }
    }

    if(versionRevField == NULL)
    {
        versionRevField = env->GetFieldID(peerClass, "versionRev", "I");
        if(versionRevField == NULL)
        {
            return NULL;
        }
    }

    if(latencyField == NULL)
    {
        latencyField = env->GetFieldID(peerClass, "latency", "I");
        if(latencyField == NULL)
        {
            return NULL;
        }
    }

    if(roleField == NULL)
    {
        roleField = env->GetFieldID(peerClass, "role", "Lcom/zerotierone/sdk/PeerRole;");
        if(roleField == NULL)
        {
            return NULL;
        }
    }

    if(pathsField == NULL)
    {
        pathsField = env->GetFieldID(peerClass, "paths", "Ljava.util.ArrayList;");
        if(pathsField == NULL)
        {
            return NULL;
        }
    }

    if(peer_constructor == NULL)
    {
        peer_constructor = env->GetMethodID(peerClass, "<init>", "()V");
        if(peer_constructor == NULL)
        {
            return NULL;
        }
    }

    jobject peerObject = env->NewObject(peerClass, peer_constructor);
    if(peerObject == NULL)
    {
        return NULL; // out of memory
    }

    env->SetLongField(peerClass, addressField, (jlong)peer.address);
    env->SetLongField(peerClass, lastUnicastFrameField, (jlong)peer.lastUnicastFrame);
    env->SetLongField(peerClass, lastMulticastFrameField, (jlong)peer.lastMulticastFrame);
    env->SetIntField(peerClass, versionMajorField, peer.versionMajor);
    env->SetIntField(peerClass, versionMinorField, peer.versionMinor);
    env->SetIntField(peerClass, versionRevField, peer.versionRev);
    env->SetIntField(peerClass, latencyField, peer.latency);
    env->SetObjectField(peerClass, roleField, createPeerRole(env, peer.role));

    jobject arrayObject = newArrayList(env);
    for(unsigned int i = 0; i < peer.pathCount; ++i)
    {
        jobject path = newPeerPhysicalPath(env, peer.paths[i]);
        appendItemToArrayList(env, arrayObject, path);
    }

    env->SetObjectField(peerClass, pathsField, arrayObject);

    return peerObject;
}

#ifdef __cplusplus
}
#endif