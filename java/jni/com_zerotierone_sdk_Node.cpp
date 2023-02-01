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

#include "ZT_jnicache.h"
#include "ZT_jniutils.h"

#include <ZeroTierOne.h>
#include "Mutex.hpp"

#include <map>
#include <string>
#include <cassert>
#include <cstring>

#define LOG_TAG "Node"

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
            , inited()
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

        bool inited;

        bool finishInitializing();
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
            return -100;
        }

        jobject operationObject = createVirtualNetworkConfigOperation(env, operation);
        if(env->ExceptionCheck() || operationObject == NULL)
        {
            return -101;
        }

        if (config == NULL) {
            LOGE("Config is NULL");
            return -102;
        }

        jobject networkConfigObject = newNetworkConfig(env, *config);
        if(env->ExceptionCheck() || networkConfigObject == NULL)
        {
            return -103;
        }

        return env->CallIntMethod(
            ref->configListener,
            VirtualNetworkConfigListener_onNetworkConfigurationUpdated_method,
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

        const unsigned char *bytes = static_cast<const unsigned char*>(frameData);
        jbyteArray dataArray = newByteArray(env, bytes, frameLength);
        if(env->ExceptionCheck() || dataArray == NULL)
        {
            return;
        }

        env->CallVoidMethod(ref->frameListener, VirtualNetworkFrameListener_onVirtualNetworkFrame_method, (jlong)nwid, (jlong)sourceMac, (jlong)destMac, (jlong)etherType, (jlong)vlanid, dataArray);
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

        jobject eventObject = createEvent(env, event);
        if (env->ExceptionCheck() || eventObject == NULL) {
            return;
        }

        switch (event) {
            case ZT_EVENT_UP: {
                LOGD("Event Up");
                env->CallVoidMethod(ref->eventListener, EventListener_onEvent_method, eventObject);
                break;
            }
            case ZT_EVENT_OFFLINE: {
                LOGD("Event Offline");
                env->CallVoidMethod(ref->eventListener, EventListener_onEvent_method, eventObject);
                break;
            }
            case ZT_EVENT_ONLINE: {
                LOGD("Event Online");
                env->CallVoidMethod(ref->eventListener, EventListener_onEvent_method, eventObject);
                break;
            }
            case ZT_EVENT_DOWN: {
                LOGD("Event Down");
                env->CallVoidMethod(ref->eventListener, EventListener_onEvent_method, eventObject);
                break;
            }
            case ZT_EVENT_FATAL_ERROR_IDENTITY_COLLISION: {
                LOGV("Identity Collision");
                // call onEvent()
                env->CallVoidMethod(ref->eventListener, EventListener_onEvent_method, eventObject);
            }
                break;
            case ZT_EVENT_TRACE: {
                LOGV("Trace Event");
                // call onTrace()
                if (data != NULL) {
                    const char *message = (const char *) data;
                    jstring messageStr = env->NewStringUTF(message);
                    env->CallVoidMethod(ref->eventListener, EventListener_onTrace_method, messageStr);
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

        jstring nameStr = env->NewStringUTF(p);

        if (bufferLength >= 0) {
            LOGD("JNI: Write file: %s", p);
            const unsigned char *bytes = static_cast<const unsigned char *>(buffer);
            jbyteArray bufferObj = newByteArray(env, bytes, bufferLength);
            if(env->ExceptionCheck() || bufferObj == NULL)
            {
                return;
            }

            env->CallIntMethod(ref->dataStorePutListener,
                               DataStorePutListener_onDataStorePut_method,
                               nameStr, bufferObj, secure);
        } else {
            LOGD("JNI: Delete file: %s", p);
            env->CallIntMethod(ref->dataStorePutListener, DataStorePutListener_onDelete_method, nameStr);
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
                return -100;
        }

        if (strlen(p) < 1) {
            return -101;
        }

        JniRef *ref = (JniRef*)userData;
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        if (ref->dataStoreGetListener == NULL) {
            LOGE("dataStoreGetListener is NULL");
            return -102;
        }

        jstring nameStr = env->NewStringUTF(p);
        if(nameStr == NULL)
        {
            LOGE("Error creating name string object");
            return -103; // out of memory
        }

        jbyteArray bufferObj = newByteArray(env, bufferLength);
        if(env->ExceptionCheck() || bufferObj == NULL)
        {
            return -104;
        }

        LOGV("Calling onDataStoreGet(%s, %p)", p, buffer);

        int retval = (int)env->CallLongMethod(
                ref->dataStoreGetListener,
                DataStoreGetListener_onDataStoreGet_method,
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
            return -100;
        }

        jobject remoteAddressObj = newInetSocketAddress(env, *remoteAddress);
        const unsigned char *bytes = static_cast<const unsigned char *>(buffer);
        jbyteArray bufferObj = newByteArray(env, bytes, bufferSize);
        if (env->ExceptionCheck() || bufferObj == NULL)
        {
            return -101;
        }
        
        int retval = env->CallIntMethod(ref->packetSender, PacketSender_onSendPacketRequested_method, localSocket, remoteAddressObj, bufferObj);

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

        return env->CallBooleanMethod(ref->pathChecker, PathChecker_onPathCheck_method, address, localSocket, remoteAddressObj);
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

        jobject sockAddressObject = env->CallObjectMethod(ref->pathChecker, PathChecker_onPathLookup_method, address, ss_family);
        if(sockAddressObject == NULL)
        {
            LOGE("Unable to call onPathLookup implementation");
            return false;
        }

        *result = fromSocketAddressObject(env, sockAddressObject);
        if (env->ExceptionCheck() || isSocketAddressEmpty(*result)) {
            return false;
        }

        return true;
    }

    typedef std::map<int64_t, JniRef*> NodeMap;
    NodeMap nodeMap;
    ZeroTier::Mutex nodeMapMutex;

    bool isInited(int64_t nodeId) {

        ZeroTier::Mutex::Lock lock(nodeMapMutex);
        NodeMap::iterator found = nodeMap.find(nodeId);

        if (found == nodeMap.end()) {

            //
            // not in map yet, or has been removed from map
            //
            return false;
        }
        
        JniRef *ref = found->second;

        assert(ref);

        return ref->inited;
    }

    bool JniRef::finishInitializing() {

        ZeroTier::Mutex::Lock lock(nodeMapMutex);
        NodeMap::iterator found = nodeMap.find(id);

        if (found != nodeMap.end()) {
            //
            // already in map
            //
            LOGE("Cannot finish initializing; node is already in map");
            return false;
        }

        nodeMap.insert(std::make_pair(id, this));

        assert(!inited);
        inited = true;

        return true;
    }

    ZT_Node* findNode(int64_t nodeId)
    {
        ZeroTier::Mutex::Lock lock(nodeMapMutex);
        NodeMap::iterator found = nodeMap.find(nodeId);

        assert(found != nodeMap.end());

        JniRef *ref = found->second;

        assert(ref);

        return ref->node;
    }

    JniRef *removeRef(int64_t nodeId) {

        ZeroTier::Mutex::Lock lock(nodeMapMutex);

        NodeMap::iterator found = nodeMap.find(nodeId);

        if (found == nodeMap.end()) {
            return nullptr;
        }

        JniRef *ref = found->second;

        assert(ref);

        nodeMap.erase(nodeId);

        return ref;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    setupJNICache(vm);
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
    teardownJNICache(vm);
}


/*
 * Class:     com_zerotier_sdk_Node
 * Method:    node_init
 * Signature: (JLcom/zerotier/sdk/DataStoreGetListener;Lcom/zerotier/sdk/DataStorePutListener;Lcom/zerotier/sdk/PacketSender;Lcom/zerotier/sdk/EventListener;Lcom/zerotier/sdk/VirtualNetworkFrameListener;Lcom/zerotier/sdk/VirtualNetworkConfigListener;Lcom/zerotier/sdk/PathChecker;)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_node_1init(
    JNIEnv *env, jobject obj, jlong now, jobject dataStoreGetListener,
    jobject dataStorePutListener, jobject packetSender, jobject eventListener,
    jobject frameListener, jobject configListener,
    jobject pathChecker)
{
    LOGV("Creating ZT_Node struct");
    jobject resultObject = ResultCode_RESULT_OK_enum;

    ZT_Node *node;
    JniRef *ref = new JniRef;
    ref->id = (int64_t)now;
    env->GetJavaVM(&ref->jvm);

    if(dataStoreGetListener == NULL)
    {
        return NULL;
    }
    ref->dataStoreGetListener = env->NewGlobalRef(dataStoreGetListener);

    if(dataStorePutListener == NULL)
    {
        return NULL;
    }
    ref->dataStorePutListener = env->NewGlobalRef(dataStorePutListener);

    if(packetSender == NULL)
    {
        return NULL;
    }
    ref->packetSender = env->NewGlobalRef(packetSender);

    if(frameListener == NULL)
    {
        return NULL;
    }
    ref->frameListener = env->NewGlobalRef(frameListener);

    if(configListener == NULL)
    {
        return NULL;
    }
    ref->configListener = env->NewGlobalRef(configListener);

    if(eventListener == NULL)
    {
        return NULL;
    }
    ref->eventListener = env->NewGlobalRef(eventListener);

    if(pathChecker != NULL)
    {
        ref->pathChecker = env->NewGlobalRef(pathChecker);
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
        if (env->ExceptionCheck() || resultObject == NULL) {
            return NULL;
        }

        if(node)
        {
            ZT_Node_delete(node);
            node = NULL;
        }
        delete ref;
        ref = NULL;
        return resultObject;
    }

    //
    // node is now updated
    //
    ref->node = node;

    if (!ref->finishInitializing()) {
        LOGE("finishInitializing() failed");
        return ResultCode_RESULT_FATAL_ERROR_INTERNAL_enum;
    }

    return resultObject;
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    node_isInited
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_com_zerotier_sdk_Node_node_1isInited
        (JNIEnv *env, jobject obj, jlong nodeId) {
    return isInited(nodeId);
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

    JniRef *ref = removeRef(nodeId);

    if (!ref) {
        return;
    }

    ZT_Node_delete(ref->node);

    delete ref;
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

    unsigned int nbtd_len = env->GetArrayLength(out_nextBackgroundTaskDeadline);
    if(nbtd_len < 1)
    {
        // array for next background task length has 0 elements!
        return ResultCode_RESULT_FATAL_ERROR_INTERNAL_enum;
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

    unsigned int nbtd_len = (unsigned int)env->GetArrayLength(out_nextBackgroundTaskDeadline);
    if(nbtd_len < 1)
    {
        LOGE("nbtd_len < 1");
        return ResultCode_RESULT_FATAL_ERROR_INTERNAL_enum;
    }

    int64_t now = (int64_t)in_now;

    sockaddr_storage remoteAddress = fromSocketAddressObject(env, in_remoteAddress);
    if (env->ExceptionCheck() || isSocketAddressEmpty(remoteAddress)) {
        return NULL;
    }

    unsigned int packetLength = (unsigned int)env->GetArrayLength(in_packetData);
    if(packetLength == 0)
    {
        LOGE("Empty packet?!?");
        return ResultCode_RESULT_FATAL_ERROR_INTERNAL_enum;
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

    unsigned int nbtd_len = env->GetArrayLength(out_nextBackgroundTaskDeadline);
    if(nbtd_len < 1)
    {
        return ResultCode_RESULT_FATAL_ERROR_INTERNAL_enum;
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

    ZT_NodeStatus nodeStatus;
    ZT_Node_status(node, &nodeStatus);

    return newNodeStatus(env, nodeStatus);
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

    ZT_PeerList *peerList = ZT_Node_peers(node);

    if(peerList == NULL)
    {
        LOGE("ZT_Node_peers returned NULL");
        return NULL;
    }

    jobjectArray peerArrayObj = newPeerArray(env, peerList->peers, peerList->peerCount);

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

    ZT_VirtualNetworkList *networkList = ZT_Node_networks(node);
    if(networkList == NULL)
    {
        return NULL;
    }

    jobjectArray networkListObject = newVirtualNetworkConfigArray(env, networkList->networks, networkList->networkCount);

    ZT_Node_freeQueryResult(node, networkList);

    return networkListObject;
}

#ifdef __cplusplus
} // extern "C"
#endif
