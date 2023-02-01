/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include "com_zerotierone_sdk_Node.h"
#include "ZT_jniutils.h"
#include "ZT_jnilookup.h"

#include <ZeroTierOne.h>
#include "Mutex.hpp"

#include <map>
#include <string>
#include <cassert>
#include <cstring>

// global static JNI Lookup Object
JniLookup lookup;

namespace {
    struct JniRef
    {
        JniRef()
            : jvm(NULL)
            , node(NULL)
            , dataStoreGetListener(NULL)
            , dataStorePutListener(NULL)
            , packetSender(NULL)
            , eventListener(NULL)
            , frameListener(NULL)
            , configListener(NULL)
            , pathChecker(NULL)
            , callbacks(NULL)
        {
            callbacks = (ZT_Node_Callbacks*)malloc(sizeof(ZT_Node_Callbacks));
            memset(callbacks, 0, sizeof(ZT_Node_Callbacks));
        }

        ~JniRef()
        {
            JNIEnv *env = NULL;
            jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

            env->DeleteGlobalRef(dataStoreGetListener);
            env->DeleteGlobalRef(dataStorePutListener);
            env->DeleteGlobalRef(packetSender);
            env->DeleteGlobalRef(eventListener);
            env->DeleteGlobalRef(frameListener);
            env->DeleteGlobalRef(configListener);
            env->DeleteGlobalRef(pathChecker);

            free(callbacks);
            callbacks = NULL;
        }

        int64_t id;

        JavaVM *jvm;

        ZT_Node *node;

        jobject dataStoreGetListener;
        jobject dataStorePutListener;
        jobject packetSender;
        jobject eventListener;
        jobject frameListener;
        jobject configListener;
        jobject pathChecker;

        ZT_Node_Callbacks *callbacks;
    };


    int VirtualNetworkConfigFunctionCallback(
        ZT_Node *node,
        void *userData,
        void *threadData,
        uint64_t nwid,
        void **,
        enum ZT_VirtualNetworkConfigOperation operation,
        const ZT_VirtualNetworkConfig *config)
    {
        LOGV("VirtualNetworkConfigFunctionCallback");
        JniRef *ref = (JniRef*)userData;
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        if (ref->configListener == NULL) {
            LOGE("configListener is NULL");
            return -1;
        }

        jclass configListenerClass = env->GetObjectClass(ref->configListener);
        if(configListenerClass == NULL)
        {
            LOGE("Couldn't find class for VirtualNetworkConfigListener instance");
            return -1;
        }

        jmethodID configListenerCallbackMethod = lookup.findMethod(configListenerClass,
            "onNetworkConfigurationUpdated",
            "(JLcom/zerotier/sdk/VirtualNetworkConfigOperation;Lcom/zerotier/sdk/VirtualNetworkConfig;)I");
        if(configListenerCallbackMethod == NULL)
        {
            LOGE("Couldn't find onVirtualNetworkFrame() method");
            return -2;
        }

        jobject operationObject = createVirtualNetworkConfigOperation(env, operation);
        if(operationObject == NULL)
        {
            LOGE("Error creating VirtualNetworkConfigOperation object");
            return -3;
        }

        jobject networkConfigObject = newNetworkConfig(env, *config);
        if(networkConfigObject == NULL)
        {
            LOGE("Error creating VirtualNetworkConfig object");
            return -4;
        }

        return env->CallIntMethod(
            ref->configListener,
            configListenerCallbackMethod,
            (jlong)nwid, operationObject, networkConfigObject);
    }

    void VirtualNetworkFrameFunctionCallback(ZT_Node *node,
        void *userData,
        void *threadData,
        uint64_t nwid,
        void**,
        uint64_t sourceMac,
        uint64_t destMac,
        unsigned int etherType,
        unsigned int vlanid,
        const void *frameData,
        unsigned int frameLength)
    {
        LOGV("VirtualNetworkFrameFunctionCallback");
#ifndef NDEBUG
        unsigned char* local = (unsigned char*)frameData;
        LOGV("Type Bytes: 0x%02x%02x", local[12], local[13]);
#endif
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        if (ref->frameListener == NULL) {
            LOGE("frameListener is NULL");
            return;
        }

        jclass frameListenerClass = env->GetObjectClass(ref->frameListener);
        if(env->ExceptionCheck() || frameListenerClass == NULL)
        {
            LOGE("Couldn't find class for VirtualNetworkFrameListener instance");
            return;
        }

        jmethodID frameListenerCallbackMethod = lookup.findMethod(
            frameListenerClass,
            "onVirtualNetworkFrame", "(JJJJJ[B)V");
        if(env->ExceptionCheck() || frameListenerCallbackMethod == NULL)
        {
            LOGE("Couldn't find onVirtualNetworkFrame() method");
            return;
        }

        jbyteArray dataArray = env->NewByteArray(frameLength);
        if(env->ExceptionCheck() || dataArray == NULL)
        {
            LOGE("Couldn't create frame data array");
            return;
        }

        void *data = env->GetPrimitiveArrayCritical(dataArray, NULL);
        memcpy(data, frameData, frameLength);
        env->ReleasePrimitiveArrayCritical(dataArray, data, 0);

        if(env->ExceptionCheck())
        {
            LOGE("Error setting frame data to array");
            return;
        }

        env->CallVoidMethod(ref->frameListener, frameListenerCallbackMethod, (jlong)nwid, (jlong)sourceMac, (jlong)destMac, (jlong)etherType, (jlong)vlanid, dataArray);
    }


    void EventCallback(ZT_Node *node,
        void *userData,
        void *threadData,
        enum ZT_Event event,
        const void *data) {
        LOGV("EventCallback");
        JniRef *ref = (JniRef *) userData;
        if (ref->node != node && event != ZT_EVENT_UP) {
            LOGE("Nodes not equal. ref->node %p, node %p. Event: %d", ref->node, node, event);
            return;
        }
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void **) &env, JNI_VERSION_1_6);

        if (ref->eventListener == NULL) {
            LOGE("eventListener is NULL");
            return;
        }

        jclass eventListenerClass = env->GetObjectClass(ref->eventListener);
        if (eventListenerClass == NULL) {
            LOGE("Couldn't class for EventListener instance");
            return;
        }

        jmethodID onEventMethod = lookup.findMethod(eventListenerClass,
                                                    "onEvent", "(Lcom/zerotier/sdk/Event;)V");
        if (onEventMethod == NULL) {
            LOGE("Couldn't find onEvent method");
            return;
        }

        jmethodID onTraceMethod = lookup.findMethod(eventListenerClass,
                                                    "onTrace", "(Ljava/lang/String;)V");
        if (onTraceMethod == NULL) {
            LOGE("Couldn't find onTrace method");
            return;
        }

        jobject eventObject = createEvent(env, event);
        if (eventObject == NULL) {
            return;
        }

        switch (event) {
            case ZT_EVENT_UP: {
                LOGD("Event Up");
                env->CallVoidMethod(ref->eventListener, onEventMethod, eventObject);
                break;
            }
            case ZT_EVENT_OFFLINE: {
                LOGD("Event Offline");
                env->CallVoidMethod(ref->eventListener, onEventMethod, eventObject);
                break;
            }
            case ZT_EVENT_ONLINE: {
                LOGD("Event Online");
                env->CallVoidMethod(ref->eventListener, onEventMethod, eventObject);
                break;
            }
            case ZT_EVENT_DOWN: {
                LOGD("Event Down");
                env->CallVoidMethod(ref->eventListener, onEventMethod, eventObject);
                break;
            }
            case ZT_EVENT_FATAL_ERROR_IDENTITY_COLLISION: {
                LOGV("Identity Collision");
                // call onEvent()
                env->CallVoidMethod(ref->eventListener, onEventMethod, eventObject);
            }
                break;
            case ZT_EVENT_TRACE: {
                LOGV("Trace Event");
                // call onTrace()
                if (data != NULL) {
                    const char *message = (const char *) data;
                    jstring messageStr = env->NewStringUTF(message);
                    env->CallVoidMethod(ref->eventListener, onTraceMethod, messageStr);
                }
            }
                break;
            case ZT_EVENT_USER_MESSAGE:
            case ZT_EVENT_REMOTE_TRACE:
            default:
                break;
        }
    }

    void StatePutFunction(
            ZT_Node *node,
            void *userData,
            void *threadData,
            enum ZT_StateObjectType type,
            const uint64_t id[2],
            const void *buffer,
            int bufferLength) {
        char p[4096] = {0};
        bool secure = false;
        switch (type) {
            case ZT_STATE_OBJECT_IDENTITY_PUBLIC:
                snprintf(p, sizeof(p), "identity.public");
                break;
            case ZT_STATE_OBJECT_IDENTITY_SECRET:
                snprintf(p, sizeof(p), "identity.secret");
                secure = true;
                break;
            case ZT_STATE_OBJECT_PLANET:
                snprintf(p, sizeof(p), "planet");
                break;
            case ZT_STATE_OBJECT_MOON:
                snprintf(p, sizeof(p), "moons.d/%.16llx.moon", (unsigned long long)id[0]);
                break;
            case ZT_STATE_OBJECT_NETWORK_CONFIG:
                snprintf(p, sizeof(p), "networks.d/%.16llx.conf", (unsigned long long)id[0]);
                break;
            case ZT_STATE_OBJECT_PEER:
                snprintf(p, sizeof(p), "peers.d/%.10llx", (unsigned long long)id[0]);
                break;
            default:
                return;
        }

        if (strlen(p) < 1) {
            return;
        }

        JniRef *ref = (JniRef*)userData;
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        if (ref->dataStorePutListener == NULL) {
            LOGE("dataStorePutListener is NULL");
            return;
        }

        jclass dataStorePutClass = env->GetObjectClass(ref->dataStorePutListener);
        if (dataStorePutClass == NULL)
        {
            LOGE("Couldn't find class for DataStorePutListener instance");
            return;
        }

        jmethodID dataStorePutCallbackMethod = lookup.findMethod(
                dataStorePutClass,
                "onDataStorePut",
                "(Ljava/lang/String;[BZ)I");
        if(dataStorePutCallbackMethod == NULL)
        {
            LOGE("Couldn't find onDataStorePut method");
            return;
        }

        jmethodID deleteMethod = lookup.findMethod(dataStorePutClass,
                                                   "onDelete", "(Ljava/lang/String;)I");
        if(deleteMethod == NULL)
        {
            LOGE("Couldn't find onDelete method");
            return;
        }

        jstring nameStr = env->NewStringUTF(p);

        if (bufferLength >= 0) {
            LOGD("JNI: Write file: %s", p);
            // set operation
            jbyteArray bufferObj = env->NewByteArray(bufferLength);
            if(env->ExceptionCheck() || bufferObj == NULL)
            {
                LOGE("Error creating byte array buffer!");
                return;
            }

            env->SetByteArrayRegion(bufferObj, 0, bufferLength, (jbyte*)buffer);

            env->CallIntMethod(ref->dataStorePutListener,
                               dataStorePutCallbackMethod,
                               nameStr, bufferObj, secure);
        } else {
            LOGD("JNI: Delete file: %s", p);
            env->CallIntMethod(ref->dataStorePutListener, deleteMethod, nameStr);
        }
    }

    int StateGetFunction(
            ZT_Node *node,
            void *userData,
            void *threadData,
            ZT_StateObjectType type,
            const uint64_t id[2],
            void *buffer,
            unsigned int bufferLength) {
        char p[4096] = {0};
        switch (type) {
            case ZT_STATE_OBJECT_IDENTITY_PUBLIC:
                snprintf(p, sizeof(p), "identity.public");
                break;
            case ZT_STATE_OBJECT_IDENTITY_SECRET:
                snprintf(p, sizeof(p), "identity.secret");
                break;
            case ZT_STATE_OBJECT_PLANET:
                snprintf(p, sizeof(p), "planet");
                break;
            case ZT_STATE_OBJECT_MOON:
                snprintf(p, sizeof(p), "moons.d/%.16llx.moon", (unsigned long long)id[0]);
                break;
            case ZT_STATE_OBJECT_NETWORK_CONFIG:
                snprintf(p, sizeof(p), "networks.d/%.16llx.conf", (unsigned long long)id[0]);
                break;
            case ZT_STATE_OBJECT_PEER:
                snprintf(p, sizeof(p), "peers.d/%.10llx", (unsigned long long)id[0]);
                break;
            default:
                return -1;
        }

        if (strlen(p) < 1) {
            return -1;
        }

        JniRef *ref = (JniRef*)userData;
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        if (ref->dataStoreGetListener == NULL) {
            LOGE("dataStoreGetListener is NULL");
            return -2;
        }

        jclass dataStoreGetClass = env->GetObjectClass(ref->dataStoreGetListener);
        if(dataStoreGetClass == NULL)
        {
            LOGE("Couldn't find class for DataStoreGetListener instance");
            return -2;
        }

        jmethodID dataStoreGetCallbackMethod = lookup.findMethod(
                dataStoreGetClass,
                "onDataStoreGet",
                "(Ljava/lang/String;[B)J");
        if(dataStoreGetCallbackMethod == NULL)
        {
            LOGE("Couldn't find onDataStoreGet method");
            return -2;
        }

        jstring nameStr = env->NewStringUTF(p);
        if(nameStr == NULL)
        {
            LOGE("Error creating name string object");
            return -2; // out of memory
        }

        jbyteArray bufferObj = env->NewByteArray(bufferLength);
        if(bufferObj == NULL)
        {
            LOGE("Error creating byte[] buffer of size: %u", bufferLength);
            return -2;
        }

        LOGV("Calling onDataStoreGet(%s, %p)", p, buffer);

        int retval = (int)env->CallLongMethod(
                ref->dataStoreGetListener,
                dataStoreGetCallbackMethod,
                nameStr,
                bufferObj);

        LOGV("onDataStoreGet returned %d", retval);

        if(retval > 0)
        {
            void *data = env->GetPrimitiveArrayCritical(bufferObj, NULL);
            memcpy(buffer, data, retval);
            env->ReleasePrimitiveArrayCritical(bufferObj, data, 0);
        }

        return retval;
    }

    int WirePacketSendFunction(ZT_Node *node,
        void *userData,
        void *threadData,
        int64_t localSocket,
        const struct sockaddr_storage *remoteAddress,
        const void *buffer,
        unsigned int bufferSize,
        unsigned int ttl)
    {
        LOGV("WirePacketSendFunction(%lld, %p, %p, %d)", (long long)localSocket, remoteAddress, buffer, bufferSize);
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);

        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        if (ref->packetSender == NULL) {
            LOGE("packetSender is NULL");
            return -1;
        }

        jclass packetSenderClass = env->GetObjectClass(ref->packetSender);
        if(packetSenderClass == NULL)
        {
            LOGE("Couldn't find class for PacketSender instance");
            return -1;
        }

        jmethodID packetSenderCallbackMethod = lookup.findMethod(packetSenderClass,
            "onSendPacketRequested", "(JLjava/net/InetSocketAddress;[BI)I");
        if(packetSenderCallbackMethod == NULL)
        {
            LOGE("Couldn't find onSendPacketRequested method");
            return -2;
        }

        jobject remoteAddressObj = newInetSocketAddress(env, *remoteAddress);
        jbyteArray bufferObj = env->NewByteArray(bufferSize);
        env->SetByteArrayRegion(bufferObj, 0, bufferSize, (jbyte*)buffer);
        int retval = env->CallIntMethod(ref->packetSender, packetSenderCallbackMethod, localSocket, remoteAddressObj, bufferObj);

        LOGV("JNI Packet Sender returned: %d", retval);
        return retval;
    }

    int PathCheckFunction(ZT_Node *node,
        void *userPtr,
        void *threadPtr,
        uint64_t address,
        int64_t localSocket,
        const struct sockaddr_storage *remoteAddress)
    {
        JniRef *ref = (JniRef*)userPtr;
        assert(ref->node == node);

        if(ref->pathChecker == NULL) {
            return true;
        }

        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        jclass pathCheckerClass = env->GetObjectClass(ref->pathChecker);
        if(pathCheckerClass == NULL)
        {
            LOGE("Couldn't find class for PathChecker instance");
            return true;
        }

        jmethodID pathCheckCallbackMethod = lookup.findMethod(pathCheckerClass,
            "onPathCheck", "(JJLjava/net/InetSocketAddress;)Z");
        if(pathCheckCallbackMethod == NULL)
        {
            LOGE("Couldn't find onPathCheck method implementation");
            return true;
        }

        //
        // was:
        // struct sockaddr_storage nullAddress = {0};
        //
        // but was getting this warning:
        // warning: suggest braces around initialization of subobject
        //
        // when building ZeroTierOne
        //
        struct sockaddr_storage nullAddress;

        //
        // It is possible to assume knowledge about internals of sockaddr_storage and construct
        // correct 0-initializer, but it is simpler to just treat sockaddr_storage as opaque and
        // use memset here to fill with 0
        //
        // This is also done in InetAddress.hpp for InetAddress
        //
        memset(&nullAddress, 0, sizeof(sockaddr_storage));

        jobject remoteAddressObj = NULL;

        if(memcmp(remoteAddress, &nullAddress, sizeof(sockaddr_storage)) != 0)
        {
            remoteAddressObj = newInetSocketAddress(env, *remoteAddress);
        }

        return env->CallBooleanMethod(ref->pathChecker, pathCheckCallbackMethod, address, localSocket, remoteAddressObj);
    }

    int PathLookupFunction(ZT_Node *node,
        void *userPtr,
        void *threadPtr,
        uint64_t address,
        int ss_family,
        struct sockaddr_storage *result)
    {
        JniRef *ref = (JniRef*)userPtr;
        assert(ref->node == node);

        if(ref->pathChecker == NULL) {
            return false;
        }

        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        jclass pathCheckerClass = env->GetObjectClass(ref->pathChecker);
        if(pathCheckerClass == NULL)
        {
            LOGE("Couldn't find class for PathChecker instance");
            return false;
        }

        jmethodID pathLookupMethod = lookup.findMethod(pathCheckerClass,
            "onPathLookup", "(JI)Ljava/net/InetSocketAddress;");
        if(pathLookupMethod == NULL) {
            return false;
        }

        jobject sockAddressObject = env->CallObjectMethod(ref->pathChecker, pathLookupMethod, address, ss_family);
        if(sockAddressObject == NULL)
        {
            LOGE("Unable to call onPathLookup implementation");
            return false;
        }

        jclass inetSockAddressClass = env->GetObjectClass(sockAddressObject);
        if(inetSockAddressClass == NULL)
        {
            LOGE("Unable to find InetSocketAddress class");
            return false;
        }

        jmethodID getAddressMethod = lookup.findMethod(inetSockAddressClass, "getAddress", "()Ljava/net/InetAddress;");
        if(getAddressMethod == NULL)
        {
            LOGE("Unable to find InetSocketAddress.getAddress() method");
            return false;
        }

        jmethodID getPortMethod = lookup.findMethod(inetSockAddressClass, "getPort", "()I");
        if(getPortMethod == NULL)
        {
            LOGE("Unable to find InetSocketAddress.getPort() method");
            return false;
        }

        jint port = env->CallIntMethod(sockAddressObject, getPortMethod);
        jobject addressObject = env->CallObjectMethod(sockAddressObject, getAddressMethod);
        
        jclass inetAddressClass = lookup.findClass("java/net/InetAddress");
        if(inetAddressClass == NULL)
        {
            LOGE("Unable to find InetAddress class");
            return false;
        }

        getAddressMethod = lookup.findMethod(inetAddressClass, "getAddress", "()[B");
        if(getAddressMethod == NULL)
        {
            LOGE("Unable to find InetAddress.getAddress() method");
            return false;
        }

        jbyteArray addressBytes = (jbyteArray)env->CallObjectMethod(addressObject, getAddressMethod);
        if(addressBytes == NULL)
        {
            LOGE("Unable to call InetAddress.getBytes()");
            return false;
        }

        int addressSize = env->GetArrayLength(addressBytes);
        if(addressSize == 4)
        {
            // IPV4
            sockaddr_in *addr = (sockaddr_in*)result;
            addr->sin_family = AF_INET;
            addr->sin_port = htons(port);
            
            void *data = env->GetPrimitiveArrayCritical(addressBytes, NULL);
            memcpy(&addr->sin_addr, data, 4);
            env->ReleasePrimitiveArrayCritical(addressBytes, data, 0);
        }
        else if (addressSize == 16)
        {
            // IPV6
            sockaddr_in6 *addr = (sockaddr_in6*)result;
            addr->sin6_family = AF_INET6;
            addr->sin6_port = htons(port);
            void *data = env->GetPrimitiveArrayCritical(addressBytes, NULL);
            memcpy(&addr->sin6_addr, data, 16);
            env->ReleasePrimitiveArrayCritical(addressBytes, data, 0);
        }
        else
        {
            return false;
        }

        return true;
    }

    typedef std::map<int64_t, JniRef*> NodeMap;
    static NodeMap nodeMap;
    ZeroTier::Mutex nodeMapMutex;

    ZT_Node* findNode(int64_t nodeId)
    {
        ZeroTier::Mutex::Lock lock(nodeMapMutex);
        NodeMap::iterator found = nodeMap.find(nodeId);
        if(found != nodeMap.end())
        {
            JniRef *ref = found->second;
            return ref->node;
        }
        return NULL;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    lookup.setJavaVM(vm);
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{

}


/*
 * Class:     com_zerotier_sdk_Node
 * Method:    node_init
 * Signature: (J)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_node_1init(
    JNIEnv *env, jobject obj, jlong now)
{
    LOGV("Creating ZT_Node struct");
    jobject resultObject = createResultObject(env, ZT_RESULT_OK);

    ZT_Node *node;
    JniRef *ref = new JniRef;
    ref->id = (int64_t)now;
    env->GetJavaVM(&ref->jvm);

    jclass cls = env->GetObjectClass(obj);
    jfieldID fid = lookup.findField(
        cls, "getListener", "Lcom/zerotier/sdk/DataStoreGetListener;");

    if(fid == NULL)
    {
        return NULL; // exception already thrown
    }

    jobject tmp = env->GetObjectField(obj, fid);
    if(tmp == NULL)
    {
        return NULL;
    }
    ref->dataStoreGetListener = env->NewGlobalRef(tmp);

    fid = lookup.findField(
        cls, "putListener", "Lcom/zerotier/sdk/DataStorePutListener;");

    if(fid == NULL)
    {
        return NULL; // exception already thrown
    }

    tmp = env->GetObjectField(obj, fid);
    if(tmp == NULL)
    {
        return NULL;
    }
    ref->dataStorePutListener = env->NewGlobalRef(tmp);

    fid = lookup.findField(
        cls, "sender", "Lcom/zerotier/sdk/PacketSender;");
    if(fid == NULL)
    {
        return NULL; // exception already thrown
    }

    tmp = env->GetObjectField(obj, fid);
    if(tmp == NULL)
    {
        return NULL;
    }
    ref->packetSender = env->NewGlobalRef(tmp);

    fid = lookup.findField(
        cls, "frameListener", "Lcom/zerotier/sdk/VirtualNetworkFrameListener;");
    if(fid == NULL)
    {
        return NULL; // exception already thrown
    }

    tmp = env->GetObjectField(obj, fid);
    if(tmp == NULL)
    {
        return NULL;
    }
    ref->frameListener = env->NewGlobalRef(tmp);

    fid = lookup.findField(
        cls, "configListener", "Lcom/zerotier/sdk/VirtualNetworkConfigListener;");
    if(fid == NULL)
    {
        return NULL; // exception already thrown
    }

    tmp = env->GetObjectField(obj, fid);
    if(tmp == NULL)
    {
        return NULL;
    }
    ref->configListener = env->NewGlobalRef(tmp);

    fid = lookup.findField(
        cls, "eventListener", "Lcom/zerotier/sdk/EventListener;");
    if(fid == NULL)
    {
        return NULL;
    }

    tmp = env->GetObjectField(obj, fid);
    if(tmp == NULL)
    {
        return NULL;
    }
    ref->eventListener = env->NewGlobalRef(tmp);

    fid = lookup.findField(
        cls, "pathChecker", "Lcom/zerotier/sdk/PathChecker;");
    if(fid == NULL)
    {
        LOGE("no path checker?");
        return NULL;
    }

    tmp = env->GetObjectField(obj, fid);
    if(tmp != NULL)
    {
        ref->pathChecker = env->NewGlobalRef(tmp);
    }

    ref->callbacks->stateGetFunction = &StateGetFunction;
    ref->callbacks->statePutFunction = &StatePutFunction;
    ref->callbacks->wirePacketSendFunction = &WirePacketSendFunction;
    ref->callbacks->virtualNetworkFrameFunction = &VirtualNetworkFrameFunctionCallback;
    ref->callbacks->virtualNetworkConfigFunction = &VirtualNetworkConfigFunctionCallback;
    ref->callbacks->eventCallback = &EventCallback;
    ref->callbacks->pathCheckFunction = &PathCheckFunction;
    ref->callbacks->pathLookupFunction = &PathLookupFunction;

    ZT_ResultCode rc = ZT_Node_new(
        &node,
        ref,
        NULL,
        ref->callbacks,
        (int64_t)now);

    if(rc != ZT_RESULT_OK)
    {
        LOGE("Error creating Node: %d", rc);
        resultObject = createResultObject(env, rc);
        if(node)
        {
            ZT_Node_delete(node);
            node = NULL;
        }
        delete ref;
        ref = NULL;
        return resultObject;
    }

    ZeroTier::Mutex::Lock lock(nodeMapMutex);
    ref->node = node;
    nodeMap.insert(std::make_pair(ref->id, ref));

    return resultObject;
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    node_delete
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_zerotier_sdk_Node_node_1delete(
    JNIEnv *env, jobject obj, jlong id)
{
    LOGV("Destroying ZT_Node struct");
    int64_t nodeId = (int64_t)id;

    NodeMap::iterator found;
    {
        ZeroTier::Mutex::Lock lock(nodeMapMutex);
        found = nodeMap.find(nodeId);
    }

    if(found != nodeMap.end())
    {
        JniRef *ref = found->second;
        nodeMap.erase(found);

        ZT_Node_delete(ref->node);

        delete ref;
        ref = NULL;
    }
    else
    {
        LOGE("Attempted to delete a node that doesn't exist!");
    }
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    processVirtualNetworkFrame
 * Signature: (JJJJJII[B[J)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_processVirtualNetworkFrame(
    JNIEnv *env, jobject obj,
    jlong id,
    jlong in_now,
    jlong in_nwid,
    jlong in_sourceMac,
    jlong in_destMac,
    jint in_etherType,
    jint in_vlanId,
    jbyteArray in_frameData,
    jlongArray out_nextBackgroundTaskDeadline)
{
    int64_t nodeId = (int64_t) id;

    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    unsigned int nbtd_len = env->GetArrayLength(out_nextBackgroundTaskDeadline);
    if(nbtd_len < 1)
    {
        // array for next background task length has 0 elements!
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    int64_t now = (int64_t)in_now;
    uint64_t nwid = (uint64_t)in_nwid;
    uint64_t sourceMac = (uint64_t)in_sourceMac;
    uint64_t destMac = (uint64_t)in_destMac;
    unsigned int etherType = (unsigned int)in_etherType;
    unsigned int vlanId = (unsigned int)in_vlanId;

    unsigned int frameLength = env->GetArrayLength(in_frameData);
    void *frameData = env->GetPrimitiveArrayCritical(in_frameData, NULL);
    void *localData = malloc(frameLength);
    memcpy(localData, frameData, frameLength);
    env->ReleasePrimitiveArrayCritical(in_frameData, frameData, 0);

    int64_t nextBackgroundTaskDeadline = 0;

    ZT_ResultCode rc = ZT_Node_processVirtualNetworkFrame(
        node,
        NULL,
        now,
        nwid,
        sourceMac,
        destMac,
        etherType,
        vlanId,
        (const void*)localData,
        frameLength,
        &nextBackgroundTaskDeadline);

    free(localData);

    jlong *outDeadline = (jlong*)env->GetPrimitiveArrayCritical(out_nextBackgroundTaskDeadline, NULL);
    outDeadline[0] = (jlong)nextBackgroundTaskDeadline;
    env->ReleasePrimitiveArrayCritical(out_nextBackgroundTaskDeadline, outDeadline, 0);

    return createResultObject(env, rc);
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    processWirePacket
 * Signature: (JJJLjava/net/InetSocketAddress;[B[J)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_processWirePacket(
    JNIEnv *env, jobject obj,
    jlong id,
    jlong in_now,
    jlong in_localSocket,
    jobject in_remoteAddress,
    jbyteArray in_packetData,
    jlongArray out_nextBackgroundTaskDeadline)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        LOGE("Couldn't find a valid node!");
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    unsigned int nbtd_len = (unsigned int)env->GetArrayLength(out_nextBackgroundTaskDeadline);
    if(nbtd_len < 1)
    {
        LOGE("nbtd_len < 1");
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    int64_t now = (int64_t)in_now;

    // get the java.net.InetSocketAddress class and getAddress() method
    jclass inetAddressClass = lookup.findClass("java/net/InetAddress");
    if(inetAddressClass == NULL)
    {
        LOGE("Can't find InetAddress class");
        // can't find java.net.InetAddress
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    jmethodID getAddressMethod = lookup.findMethod(
        inetAddressClass, "getAddress", "()[B");
    if(getAddressMethod == NULL)
    {
        // cant find InetAddress.getAddress()
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    jclass InetSocketAddressClass = lookup.findClass("java/net/InetSocketAddress");
    if(InetSocketAddressClass == NULL)
    {
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    jmethodID inetSockGetAddressMethod = lookup.findMethod(
        InetSocketAddressClass, "getAddress", "()Ljava/net/InetAddress;");

    jobject remoteAddrObject = env->CallObjectMethod(in_remoteAddress, inetSockGetAddressMethod);

    if(remoteAddrObject == NULL)
    {
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    jmethodID inetSock_getPort = lookup.findMethod(
        InetSocketAddressClass, "getPort", "()I");

    if(env->ExceptionCheck() || inetSock_getPort == NULL)
    {
        LOGE("Couldn't find getPort method on InetSocketAddress");
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    // call InetSocketAddress.getPort()
    int remotePort = env->CallIntMethod(in_remoteAddress, inetSock_getPort);
    if(env->ExceptionCheck())
    {
        LOGE("Exception calling InetSocketAddress.getPort()");
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    // Call InetAddress.getAddress()
    jbyteArray remoteAddressArray = (jbyteArray)env->CallObjectMethod(remoteAddrObject, getAddressMethod);
    if(remoteAddressArray == NULL)
    {
        LOGE("Unable to call getAddress()");
        // unable to call getAddress()
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    unsigned int addrSize = env->GetArrayLength(remoteAddressArray);


    // get the address bytes
    jbyte *addr = (jbyte*)env->GetPrimitiveArrayCritical(remoteAddressArray, NULL);
    sockaddr_storage remoteAddress = {};

    if(addrSize == 16)
    {
        // IPV6 address
        sockaddr_in6 ipv6 = {};
        ipv6.sin6_family = AF_INET6;
        ipv6.sin6_port = htons(remotePort);
        memcpy(ipv6.sin6_addr.s6_addr, addr, 16);
        memcpy(&remoteAddress, &ipv6, sizeof(sockaddr_in6));
    }
    else if(addrSize == 4)
    {
        // IPV4 address
        sockaddr_in ipv4 = {};
        ipv4.sin_family = AF_INET;
        ipv4.sin_port = htons(remotePort);
        memcpy(&ipv4.sin_addr, addr, 4);
        memcpy(&remoteAddress, &ipv4, sizeof(sockaddr_in));
    }
    else
    {
        LOGE("Unknown IP version");
        // unknown address type
        env->ReleasePrimitiveArrayCritical(remoteAddressArray, addr, 0);
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }
    env->ReleasePrimitiveArrayCritical(remoteAddressArray, addr, 0);

    unsigned int packetLength = (unsigned int)env->GetArrayLength(in_packetData);
    if(packetLength == 0)
    {
        LOGE("Empty packet?!?");
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }
    void *packetData = env->GetPrimitiveArrayCritical(in_packetData, NULL);
    void *localData = malloc(packetLength);
    memcpy(localData, packetData, packetLength);
    env->ReleasePrimitiveArrayCritical(in_packetData, packetData, 0);

    int64_t nextBackgroundTaskDeadline = 0;

    ZT_ResultCode rc = ZT_Node_processWirePacket(
        node,
        NULL,
        now,
        in_localSocket,
        &remoteAddress,
        localData,
        packetLength,
        &nextBackgroundTaskDeadline);
    if(rc != ZT_RESULT_OK)
    {
        LOGE("ZT_Node_processWirePacket returned: %d", rc);
    }

    free(localData);

    jlong *outDeadline = (jlong*)env->GetPrimitiveArrayCritical(out_nextBackgroundTaskDeadline, NULL);
    outDeadline[0] = (jlong)nextBackgroundTaskDeadline;
    env->ReleasePrimitiveArrayCritical(out_nextBackgroundTaskDeadline, outDeadline, 0);

    return createResultObject(env, rc);
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    processBackgroundTasks
 * Signature: (JJ[J)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_processBackgroundTasks(
    JNIEnv *env, jobject obj,
    jlong id,
    jlong in_now,
    jlongArray out_nextBackgroundTaskDeadline)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    unsigned int nbtd_len = env->GetArrayLength(out_nextBackgroundTaskDeadline);
    if(nbtd_len < 1)
    {
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    int64_t now = (int64_t)in_now;
    int64_t nextBackgroundTaskDeadline = 0;

    ZT_ResultCode rc = ZT_Node_processBackgroundTasks(node, NULL, now, &nextBackgroundTaskDeadline);

    jlong *outDeadline = (jlong*)env->GetPrimitiveArrayCritical(out_nextBackgroundTaskDeadline, NULL);
    outDeadline[0] = (jlong)nextBackgroundTaskDeadline;
    env->ReleasePrimitiveArrayCritical(out_nextBackgroundTaskDeadline, outDeadline, 0);

    return createResultObject(env, rc);
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    join
 * Signature: (JJ)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_join(
    JNIEnv *env, jobject obj, jlong id, jlong in_nwid)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t nwid = (uint64_t)in_nwid;

    ZT_ResultCode rc = ZT_Node_join(node, nwid, NULL, NULL);

    return createResultObject(env, rc);
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    leave
 * Signature: (JJ)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_leave(
    JNIEnv *env, jobject obj, jlong id, jlong in_nwid)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t nwid = (uint64_t)in_nwid;

    ZT_ResultCode rc = ZT_Node_leave(node, nwid, NULL, NULL);

    return createResultObject(env, rc);
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    multicastSubscribe
 * Signature: (JJJJ)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_multicastSubscribe(
    JNIEnv *env, jobject obj,
    jlong id,
    jlong in_nwid,
    jlong in_multicastGroup,
    jlong in_multicastAdi)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t nwid = (uint64_t)in_nwid;
    uint64_t multicastGroup = (uint64_t)in_multicastGroup;
    unsigned long multicastAdi = (unsigned long)in_multicastAdi;

    ZT_ResultCode rc = ZT_Node_multicastSubscribe(
        node, NULL, nwid, multicastGroup, multicastAdi);

    return createResultObject(env, rc);
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    multicastUnsubscribe
 * Signature: (JJJJ)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_multicastUnsubscribe(
    JNIEnv *env, jobject obj,
    jlong id,
    jlong in_nwid,
    jlong in_multicastGroup,
    jlong in_multicastAdi)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t nwid = (uint64_t)in_nwid;
    uint64_t multicastGroup = (uint64_t)in_multicastGroup;
    unsigned long multicastAdi = (unsigned long)in_multicastAdi;

    ZT_ResultCode rc = ZT_Node_multicastUnsubscribe(
        node, nwid, multicastGroup, multicastAdi);

    return createResultObject(env, rc);
}

/*
 * Class:   com_zerotier_sdk_Node
 * Method:  orbit
 * Signature: (JJJ)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_orbit(
    JNIEnv *env, jobject obj,
    jlong id,
    jlong in_moonWorldId,
    jlong in_moonSeed)
{
    int64_t nodeId = (int64_t)id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t moonWorldId = (uint64_t)in_moonWorldId;
    uint64_t moonSeed = (uint64_t)in_moonSeed;

    ZT_ResultCode rc = ZT_Node_orbit(node, NULL, moonWorldId, moonSeed);
    return createResultObject(env, rc);
}

/*
 * Class:   com_zerotier_sdk_Node
 * Method:  deorbit
 * Signature: (JJ)L/com/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_deorbit(
    JNIEnv *env, jobject obj,
    jlong id,
    jlong in_moonWorldId)
{
    int64_t nodeId = (int64_t)id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        return createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t moonWorldId = (uint64_t)in_moonWorldId;

    ZT_ResultCode rc = ZT_Node_deorbit(node, NULL, moonWorldId);
    return createResultObject(env, rc);
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    address
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_com_zerotier_sdk_Node_address(
    JNIEnv *env , jobject obj, jlong id)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    uint64_t address = ZT_Node_address(node);
    return (jlong)address;
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    status
 * Signature: (J)Lcom/zerotier/sdk/NodeStatus;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_status
   (JNIEnv *env, jobject obj, jlong id)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    jclass nodeStatusClass = NULL;
    jmethodID nodeStatusConstructor = NULL;

    // create a com.zerotier.sdk.NodeStatus object
    nodeStatusClass = lookup.findClass("com/zerotier/sdk/NodeStatus");
    if(nodeStatusClass == NULL)
    {
        return NULL;
    }

    nodeStatusConstructor = lookup.findMethod(
        nodeStatusClass, "<init>", "()V");
    if(nodeStatusConstructor == NULL)
    {
        return NULL;
    }

    jobject nodeStatusObj = env->NewObject(nodeStatusClass, nodeStatusConstructor);
    if(nodeStatusObj == NULL)
    {
        return NULL;
    }

    ZT_NodeStatus nodeStatus;
    ZT_Node_status(node, &nodeStatus);

    jfieldID addressField = NULL;
    jfieldID publicIdentityField = NULL;
    jfieldID secretIdentityField = NULL;
    jfieldID onlineField = NULL;

    addressField = lookup.findField(nodeStatusClass, "address", "J");
    if(addressField == NULL)
    {
        return NULL;
    }

    publicIdentityField = lookup.findField(nodeStatusClass, "publicIdentity", "Ljava/lang/String;");
    if(publicIdentityField == NULL)
    {
        return NULL;
    }

    secretIdentityField = lookup.findField(nodeStatusClass, "secretIdentity", "Ljava/lang/String;");
    if(secretIdentityField == NULL)
    {
        return NULL;
    }

    onlineField = lookup.findField(nodeStatusClass, "online", "Z");
    if(onlineField == NULL)
    {
        return NULL;
    }

    env->SetLongField(nodeStatusObj, addressField, nodeStatus.address);

    jstring pubIdentStr = env->NewStringUTF(nodeStatus.publicIdentity);
    if(pubIdentStr == NULL)
    {
        return NULL; // out of memory
    }
    env->SetObjectField(nodeStatusObj, publicIdentityField, pubIdentStr);

    jstring secIdentStr = env->NewStringUTF(nodeStatus.secretIdentity);
    if(secIdentStr == NULL)
    {
        return NULL; // out of memory
    }
    env->SetObjectField(nodeStatusObj, secretIdentityField, secIdentStr);

    env->SetBooleanField(nodeStatusObj, onlineField, nodeStatus.online);

    return nodeStatusObj;
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    networkConfig
 * Signature: (JJ)Lcom/zerotier/sdk/VirtualNetworkConfig;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_networkConfig(
    JNIEnv *env, jobject obj, jlong id, jlong nwid)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    ZT_VirtualNetworkConfig *vnetConfig = ZT_Node_networkConfig(node, nwid);

    jobject vnetConfigObject = newNetworkConfig(env, *vnetConfig);

    ZT_Node_freeQueryResult(node, vnetConfig);

    return vnetConfigObject;
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    version
 * Signature: ()Lcom/zerotier/sdk/Version;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_version(
    JNIEnv *env, jobject obj)
{
    int major = 0;
    int minor = 0;
    int revision = 0;

    ZT_version(&major, &minor, &revision);

    return newVersion(env, major, minor, revision);
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    peers
 * Signature: (J)[Lcom/zerotier/sdk/Peer;
 */
JNIEXPORT jobjectArray JNICALL Java_com_zerotier_sdk_Node_peers(
    JNIEnv *env, jobject obj, jlong id)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    ZT_PeerList *peerList = ZT_Node_peers(node);

    if(peerList == NULL)
    {
        LOGE("ZT_Node_peers returned NULL");
        return NULL;
    }

    jclass peerClass = lookup.findClass("com/zerotier/sdk/Peer");
    if(env->ExceptionCheck() || peerClass == NULL)
    {
        LOGE("Error finding Peer class");
        ZT_Node_freeQueryResult(node, peerList);
        return NULL;
    }

    jobjectArray peerArrayObj = env->NewObjectArray(
        peerList->peerCount, peerClass, NULL);

    if(env->ExceptionCheck() || peerArrayObj == NULL)
    {
        LOGE("Error creating Peer[] array");
        ZT_Node_freeQueryResult(node, peerList);
        return NULL;
    }


    for(unsigned int i = 0; i < peerList->peerCount; ++i)
    {
        jobject peerObj = newPeer(env, peerList->peers[i]);
        env->SetObjectArrayElement(peerArrayObj, i, peerObj);
        if(env->ExceptionCheck())
        {
            LOGE("Error assigning Peer object to array");
            break;
        }

        env->DeleteLocalRef(peerObj);
    }

    ZT_Node_freeQueryResult(node, peerList);
    peerList = NULL;

    return peerArrayObj;
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    networks
 * Signature: (J)[Lcom/zerotier/sdk/VirtualNetworkConfig;
 */
JNIEXPORT jobjectArray JNICALL Java_com_zerotier_sdk_Node_networks(
    JNIEnv *env, jobject obj, jlong id)
{
    int64_t nodeId = (int64_t) id;
    ZT_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    ZT_VirtualNetworkList *networkList = ZT_Node_networks(node);
    if(networkList == NULL)
    {
        return NULL;
    }

    jclass vnetConfigClass = lookup.findClass("com/zerotier/sdk/VirtualNetworkConfig");
    if(env->ExceptionCheck() || vnetConfigClass == NULL)
    {
        LOGE("Error finding VirtualNetworkConfig class");
        ZT_Node_freeQueryResult(node, networkList);
        return NULL;
    }

    jobjectArray networkListObject = env->NewObjectArray(
        networkList->networkCount, vnetConfigClass, NULL);
    if(env->ExceptionCheck() || networkListObject == NULL)
    {
        LOGE("Error creating VirtualNetworkConfig[] array");
        ZT_Node_freeQueryResult(node, networkList);
        return NULL;
    }

    for(unsigned int i = 0; i < networkList->networkCount; ++i)
    {
        jobject networkObject = newNetworkConfig(env, networkList->networks[i]);
        env->SetObjectArrayElement(networkListObject, i, networkObject);
        if(env->ExceptionCheck())
        {
            LOGE("Error assigning VirtualNetworkConfig object to array");
            break;
        }

        env->DeleteLocalRef(networkObject);
    }

    ZT_Node_freeQueryResult(node, networkList);

    return networkListObject;
}

#ifdef __cplusplus
} // extern "C"
#endif
