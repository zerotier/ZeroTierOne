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
#include "ZT1_jniutils.h"
#include "ZT1_jnicache.h"

#include <ZeroTierOne.h>

#include <map>
#include <string>
#include <assert.h>
#include <string.h>

// global static JNI Cache Object
JniCache cache;

#ifdef __cplusplus
extern "C" {
#endif

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
        {}

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
        }

        uint64_t id;

        JavaVM *jvm;

        ZT1_Node *node;

        jobject dataStoreGetListener;
        jobject dataStorePutListener;
        jobject packetSender;
        jobject eventListener;
        jobject frameListener;
        jobject configListener;
    };


    int VirtualNetworkConfigFunctionCallback(
        ZT1_Node *node,
        void *userData,
        uint64_t nwid,
        enum ZT1_VirtualNetworkConfigOperation operation,
        const ZT1_VirtualNetworkConfig *config)
    {
        JniRef *ref = (JniRef*)userData;
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        jclass configListenerClass = env->GetObjectClass(ref->configListener);
        if(configListenerClass == NULL)
        {
            LOGE("Couldn't find class for VirtualNetworkConfigListener instance");
            return -1;
        }

        jmethodID configListenerCallbackMethod = cache.findMethod(configListenerClass,
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

    void VirtualNetworkFrameFunctionCallback(ZT1_Node *node,void *userData,
        uint64_t nwid,
        uint64_t sourceMac,
        uint64_t destMac,
        unsigned int etherType,
        unsigned int vlanid,
        const void *frameData,
        unsigned int frameLength)
    {
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);


        jclass frameListenerClass = env->GetObjectClass(ref->frameListener);
        if(frameListenerClass == NULL)
        {
            LOGE("Couldn't find class for VirtualNetworkFrameListener instance");
            return;
        }

        jmethodID frameListenerCallbackMethod = cache.findMethod(
            frameListenerClass,
            "onVirtualNetworkFrame", "(JJJJJ[B)V");
        if(frameListenerCallbackMethod == NULL)
        {
            LOGE("Couldn't find onVirtualNetworkFrame() method");
            return;
        }

        jbyteArray dataArray = env->NewByteArray(frameLength);
        env->SetByteArrayRegion(dataArray, 0, frameLength, (jbyte*)frameData);

        env->CallVoidMethod(ref->frameListener, frameListenerCallbackMethod, nwid, sourceMac, destMac, etherType, vlanid, dataArray);
    }


    void EventCallback(ZT1_Node *node,void *userData,enum ZT1_Event event, const void *data)
    {
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);


        jclass eventListenerClass = env->GetObjectClass(ref->eventListener);
        if(eventListenerClass == NULL)
        {
            LOGE("Couldn't class for EventListener instance");
            return;
        }

        jmethodID onEventMethod = cache.findMethod(eventListenerClass,
            "onEvent", "(Lcom/zerotier/sdk/Event;)V");
        if(onEventMethod == NULL)
        {
            LOGE("Couldn't find onEvent method");
            return;
        }


        jmethodID onOutOfDateMethod = cache.findMethod(eventListenerClass,
            "onOutOfDate", "(Lcom/zerotier/sdk/Version;)V");
        if(onOutOfDateMethod == NULL)
        {
            LOGE("Couldn't find onOutOfDate method");
            return;
        }


        jmethodID onNetworkErrorMethod = cache.findMethod(eventListenerClass,
            "onNetworkError", "(Lcom/zerotier/sdk/Event;Ljava/net/InetSocketAddress;)V");
        if(onNetworkErrorMethod == NULL)
        {
            LOGE("Couldn't find onNetworkError method");
            return;
        }


        jmethodID onTraceMethod = cache.findMethod(eventListenerClass,
            "onTrace", "(Ljava/lang/String;)V");
        if(onTraceMethod == NULL)
        {
            LOGE("Couldn't find onTrace method");
            return;
        }

        jobject eventObject = createEvent(env, event);
        if(eventObject == NULL)
        {
            return;
        }

        switch(event)
        {
        case ZT1_EVENT_UP:
        case ZT1_EVENT_OFFLINE:
        case ZT1_EVENT_ONLINE:
        case ZT1_EVENT_DOWN:
        case ZT1_EVENT_FATAL_ERROR_IDENTITY_COLLISION:
        {
            // call onEvent()
            env->CallVoidMethod(ref->eventListener, onEventMethod, eventObject);
        }
        break;
        case ZT1_EVENT_SAW_MORE_RECENT_VERSION:
        {
            // call onOutOfDate()
            if(data != NULL)
            {
                int *version = (int*)data;
                jobject verisonObj = newVersion(env, version[0], version[1], version[2], 0);
                env->CallVoidMethod(ref->eventListener, onOutOfDateMethod, verisonObj);
            }
        }
        break;
        case ZT1_EVENT_AUTHENTICATION_FAILURE:
        case ZT1_EVENT_INVALID_PACKET:
        {
            // call onNetworkError()
            if(data != NULL)
            {
                sockaddr_storage *addr = (sockaddr_storage*)data;
                jobject addressObj = newInetSocketAddress(env, *addr);
                env->CallVoidMethod(ref->eventListener, onNetworkErrorMethod, addressObj);
            }
        }
        break;
        case ZT1_EVENT_TRACE:
        {
            // call onTrace()
            if(data != NULL)
            {
                const char* message = (const char*)data;
                jstring messageStr = env->NewStringUTF(message);
                env->CallVoidMethod(ref->eventListener, onTraceMethod, messageStr);
            }
        }
        break;
        }
    }

    long DataStoreGetFunction(ZT1_Node *node,void *userData,
        const char *objectName,
        void *buffer,
        unsigned long bufferSize,
        unsigned long bufferIndex,
        unsigned long *out_objectSize)
    {
        JniRef *ref = (JniRef*)userData;
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);

        jclass dataStoreGetClass = env->GetObjectClass(ref->dataStoreGetListener);
        if(dataStoreGetClass == NULL)
        {
            LOGE("Couldn't find class for DataStoreGetListener instance");
            return -2;
        }

        jmethodID dataStoreGetCallbackMethod = cache.findMethod(
            dataStoreGetClass,
            "onDataStoreGet",
            "(Ljava/lang/String;[BJ[J)J");
        if(dataStoreGetCallbackMethod == NULL)
        {
            LOGE("Couldn't find onDataStoreGet method");
            return -2;
        }

        jstring nameStr = env->NewStringUTF(objectName);
        if(nameStr == NULL)
        {
            LOGE("Error creating name string object");
            return -2; // out of memory
        }

        jbyteArray bufferObj = env->NewByteArray(bufferSize);
        if(bufferObj == NULL)
        {
            LOGE("Error creating byte[] buffer of size: %lu", bufferSize);
            return -2;
        }

        jlongArray objectSizeObj = env->NewLongArray(1);
        if(objectSizeObj == NULL)
        {
            LOGE("Error creating long[1] array for actual object size");
            return -2; // couldn't create long[1] array
        }

        LOGV("Calling onDataStoreGet(%s, %p, %lu, %p)",
            objectName, buffer, bufferIndex, objectSizeObj);

        long retval = (long)env->CallLongMethod(
            ref->dataStoreGetListener, dataStoreGetCallbackMethod, 
            nameStr, bufferObj, (jlong)bufferIndex, objectSizeObj);

        if(retval > 0)
        {
            jbyte *data = env->GetByteArrayElements(bufferObj, NULL);
            memcpy(buffer, data, retval);
            env->ReleaseByteArrayElements(bufferObj, data, JNI_ABORT);

            jlong *objSize = env->GetLongArrayElements(objectSizeObj, NULL);
            *out_objectSize = (unsigned long)objSize[0];
            env->ReleaseLongArrayElements(objectSizeObj, objSize, JNI_ABORT);
        }

        LOGI("Out Object Size: %lu", *out_objectSize);

        return retval;
    }

    int DataStorePutFunction(ZT1_Node *node,void *userData,
        const char *objectName,
        const void *buffer,
        unsigned long bufferSize,
        int secure)
    {
        JniRef *ref = (JniRef*)userData;
        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);


        jclass dataStorePutClass = env->GetObjectClass(ref->dataStorePutListener);
        if(dataStorePutClass == NULL)
        {
            LOGE("Couldn't find class for DataStorePutListener instance");
            return -1;
        }

        jmethodID dataStorePutCallbackMethod = cache.findMethod(
            dataStorePutClass,
            "onDataStorePut",
            "(Ljava/lang/String;[BZ)I");
        if(dataStorePutCallbackMethod == NULL)
        {
            LOGE("Couldn't find onDataStorePut method");
            return -2;
        }

        jmethodID deleteMethod = cache.findMethod(dataStorePutClass,
            "onDelete", "(Ljava/lang/String;)I");
        if(deleteMethod == NULL)
        {
            LOGE("Couldn't find onDelete method");
            return -3;
        }

        jstring nameStr = env->NewStringUTF(objectName);

        if(buffer == NULL)
        {
            // delete operation
            return env->CallIntMethod(
                ref->dataStorePutListener, deleteMethod, nameStr);
        }
        else
        {
            // set operation
            jbyteArray bufferObj = env->NewByteArray(bufferSize);
            env->SetByteArrayRegion(bufferObj, 0, bufferSize, (jbyte*)buffer);
            bool bsecure = secure != 0;


            return env->CallIntMethod(ref->dataStorePutListener,
                dataStorePutCallbackMethod,
                nameStr, bufferObj, bsecure);
        }
    }

    int WirePacketSendFunction(ZT1_Node *node,void *userData,\
        const struct sockaddr_storage *address,
        const void *buffer,
        unsigned int bufferSize)
    {
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);

        JNIEnv *env = NULL;
        ref->jvm->GetEnv((void**)&env, JNI_VERSION_1_6);


        jclass packetSenderClass = env->GetObjectClass(ref->packetSender);
        if(packetSenderClass == NULL)
        {
            LOGE("Couldn't find class for PacketSender instance");
            return -1;
        }

        jmethodID packetSenderCallbackMethod = cache.findMethod(packetSenderClass,
            "onSendPacketRequested", "(Ljava/net/InetSocketAddress;[B)I");
        if(packetSenderCallbackMethod == NULL)
        {
            LOGE("Couldn't find onSendPacketRequested method");
            return -2;
        }
        
        jobject addressObj = newInetSocketAddress(env, *address);
        jbyteArray bufferObj = env->NewByteArray(bufferSize);
        env->SetByteArrayRegion(bufferObj, 0, bufferSize, (jbyte*)buffer);
        int retval = env->CallIntMethod(ref->packetSender, packetSenderCallbackMethod, addressObj, bufferObj);

        LOGD("JNI Packet Sender returned: %d", retval);
        return retval;
    }

    typedef std::map<uint64_t, JniRef*> NodeMap;
    static NodeMap nodeMap;

    ZT1_Node* findNode(uint64_t nodeId)
    {
        NodeMap::iterator found = nodeMap.find(nodeId);
        if(found != nodeMap.end())
        {
            JniRef *ref = found->second;
            return ref->node;
        }
        return NULL;
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) 
{
    cache.setJavaVM(vm);
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
    cache.clearCache();
}


/*
 * Class:     com_zerotier_sdk_Node
 * Method:    node_init
 * Signature: (J)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_node_1init(
    JNIEnv *env, jobject obj, jlong now)
{
    LOGV("Creating ZT1_Node struct");
    jobject resultObject = createResultObject(env, ZT1_RESULT_OK);

    ZT1_Node *node;
    JniRef *ref = new JniRef;
    ref->id = (uint64_t)now;
    env->GetJavaVM(&ref->jvm);

    jclass cls = env->GetObjectClass(obj);
    jfieldID fid = cache.findField(
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

    fid = cache.findField(
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

    fid = cache.findField(
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

    fid = cache.findField(
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

    fid = cache.findField(
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

    fid = cache.findField(
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

    ZT1_ResultCode rc = ZT1_Node_new(
        &node,
        ref,
        (uint64_t)now,
        &DataStoreGetFunction,
        &DataStorePutFunction,
        &WirePacketSendFunction,
        &VirtualNetworkFrameFunctionCallback,
        &VirtualNetworkConfigFunctionCallback,
        &EventCallback);

    if(rc != ZT1_RESULT_OK)
    {
        LOGE("Error creating Node: %d", rc);
        resultObject = createResultObject(env, rc);
        if(node)
        {
            ZT1_Node_delete(node);
            node = NULL;
        }
        delete ref;
        ref = NULL;
        return resultObject;
    }

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
    LOGV("Destroying ZT1_Node struct");
    uint64_t nodeId = (uint64_t)id;

    NodeMap::iterator found = nodeMap.find(nodeId);
    if(found != nodeMap.end())
    {
        JniRef *ref = found->second;
        nodeMap.erase(found);

        ZT1_Node_delete(ref->node);

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
    uint64_t nodeId = (uint64_t) id;
    
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    unsigned int nbtd_len = env->GetArrayLength(out_nextBackgroundTaskDeadline);
    if(nbtd_len < 1)
    {
        // array for next background task length has 0 elements!
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t now = (uint64_t)in_now;
    uint64_t nwid = (uint64_t)in_nwid;
    uint64_t sourceMac = (uint64_t)in_sourceMac;
    uint64_t destMac = (uint64_t)in_destMac;
    unsigned int etherType = (unsigned int)in_etherType;
    unsigned int vlanId = (unsigned int)in_vlanId;

    unsigned int frameLength = env->GetArrayLength(in_frameData);
    jbyte *frameData =env->GetByteArrayElements(in_frameData, NULL);

    uint64_t nextBackgroundTaskDeadline = 0;

    ZT1_ResultCode rc = ZT1_Node_processVirtualNetworkFrame(
        node,
        now,
        nwid,
        sourceMac,
        destMac,
        etherType,
        vlanId,
        (const void*)frameData,
        frameLength,
        &nextBackgroundTaskDeadline);

    jlong *outDeadline = env->GetLongArrayElements(out_nextBackgroundTaskDeadline, NULL);
    outDeadline[0] = (jlong)nextBackgroundTaskDeadline;
    env->ReleaseLongArrayElements(out_nextBackgroundTaskDeadline, outDeadline, 0);

    env->ReleaseByteArrayElements(in_frameData, frameData, 0);

    return createResultObject(env, rc);
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    processWirePacket
 * Signature: (JJLjava/net/InetSocketAddress;I[B[J)Lcom/zerotier/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_processWirePacket(
    JNIEnv *env, jobject obj, 
    jlong id,
    jlong in_now, 
    jobject in_remoteAddress,
    jbyteArray in_packetData,
    jlongArray out_nextBackgroundTaskDeadline)
{
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    unsigned int nbtd_len = env->GetArrayLength(out_nextBackgroundTaskDeadline);
    if(nbtd_len < 1)
    {
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t now = (uint64_t)in_now;

    // get the java.net.InetSocketAddress class and getAddress() method
    jclass inetAddressClass = cache.findClass("java/net/InetAddress");
    if(inetAddressClass == NULL)
    {
        // can't find java.net.InetAddress
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    jmethodID getAddressMethod = cache.findMethod(
        inetAddressClass, "getAddress", "()[B");
    if(getAddressMethod == NULL)
    {
        // cant find InetAddress.getAddres()
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    jclass InetSocketAddressClass = cache.findClass("java/net/InetSocketAddress");
    if(InetSocketAddressClass == NULL)
    {
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    jmethodID inetSockGetAddressMethod = cache.findMethod(
        InetSocketAddressClass, "getAddress", "()Ljava/net/InetAddress;");

    jobject addrObject = env->CallObjectMethod(in_remoteAddress, inetSockGetAddressMethod);

    if(addrObject == NULL)
    {
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    // Call InetAddress.getAddress()
    jbyteArray addressArray = (jbyteArray)env->CallObjectMethod(addrObject, getAddressMethod);
    if(addressArray == NULL)
    {
        // unable to call getAddress()
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    unsigned int addrSize = env->GetArrayLength(addressArray);
    // get the address bytes
    jbyte *addr = env->GetByteArrayElements(addressArray, NULL);


    sockaddr_storage remoteAddress = {};

    if(addrSize == 16)
    {
        // IPV6 address
        sockaddr_in6 ipv6 = {};
        ipv6.sin6_family = AF_INET6;
        memcpy(ipv6.sin6_addr.s6_addr, addr, 16);
        memcpy(&remoteAddress, &ipv6, sizeof(sockaddr_in6));
    }
    else if(addrSize == 4)
    {
        // IPV4 address
        sockaddr_in ipv4 = {};
        ipv4.sin_family = AF_INET;
        memcpy(&ipv4.sin_addr, addr, 4);
        memcpy(&remoteAddress, &ipv4, sizeof(sockaddr_in));
    }
    else
    {
        // unknown address type
        env->ReleaseByteArrayElements(addressArray, addr, 0);
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }


    unsigned int packetLength = env->GetArrayLength(in_packetData);
    jbyte *packetData = env->GetByteArrayElements(in_packetData, NULL);

    uint64_t nextBackgroundTaskDeadline = 0;

    ZT1_ResultCode rc = ZT1_Node_processWirePacket(
        node,
        now,
        &remoteAddress,
        packetData,
        packetLength,
        &nextBackgroundTaskDeadline);

    jlong *outDeadline = env->GetLongArrayElements(out_nextBackgroundTaskDeadline, NULL);
    outDeadline[0] = (jlong)nextBackgroundTaskDeadline;
    env->ReleaseLongArrayElements(out_nextBackgroundTaskDeadline, outDeadline, 0);

    env->ReleaseByteArrayElements(addressArray, addr, 0);
    env->ReleaseByteArrayElements(in_packetData, packetData, 0);

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
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    unsigned int nbtd_len = env->GetArrayLength(out_nextBackgroundTaskDeadline);
    if(nbtd_len < 1)
    {
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t now = (uint64_t)in_now;
    uint64_t nextBackgroundTaskDeadline = 0;

    ZT1_ResultCode rc = ZT1_Node_processBackgroundTasks(node, now, &nextBackgroundTaskDeadline);

    jlong *outDeadline = env->GetLongArrayElements(out_nextBackgroundTaskDeadline, NULL);
    outDeadline[0] = (jlong)nextBackgroundTaskDeadline;
    env->ReleaseLongArrayElements(out_nextBackgroundTaskDeadline, outDeadline, 0);

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
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t nwid = (uint64_t)in_nwid;

    ZT1_ResultCode rc = ZT1_Node_join(node, nwid);

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
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t nwid = (uint64_t)in_nwid;

    ZT1_ResultCode rc = ZT1_Node_leave(node, nwid);

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
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t nwid = (uint64_t)in_nwid;
    uint64_t multicastGroup = (uint64_t)in_multicastGroup;
    unsigned long multicastAdi = (unsigned long)in_multicastAdi;

    ZT1_ResultCode rc = ZT1_Node_multicastSubscribe(
        node, nwid, multicastGroup, multicastAdi);

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
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    uint64_t nwid = (uint64_t)in_nwid;
    uint64_t multicastGroup = (uint64_t)in_multicastGroup;
    unsigned long multicastAdi = (unsigned long)in_multicastAdi;

    ZT1_ResultCode rc = ZT1_Node_multicastUnsubscribe(
        node, nwid, multicastGroup, multicastAdi);

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
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    uint64_t address = ZT1_Node_address(node);
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
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    jclass nodeStatusClass = NULL;
    jmethodID nodeStatusConstructor = NULL;

    // create a com.zerotier.sdk.NodeStatus object
    nodeStatusClass = cache.findClass("com/zerotier/sdk/NodeStatus");
    if(nodeStatusClass == NULL)
    {
        return NULL;
    }
    
    nodeStatusConstructor = cache.findMethod(
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

    ZT1_NodeStatus nodeStatus;
    ZT1_Node_status(node, &nodeStatus);

    jfieldID addressField = NULL;
    jfieldID publicIdentityField = NULL;
    jfieldID secretIdentityField = NULL;
    jfieldID onlineField = NULL;

    addressField = cache.findField(nodeStatusClass, "address", "J");
    if(addressField == NULL)
    {
        return NULL;
    }

    publicIdentityField = cache.findField(nodeStatusClass, "publicIdentity", "Ljava/lang/String;");
    if(publicIdentityField == NULL)
    {
        return NULL;
    }

    secretIdentityField = cache.findField(nodeStatusClass, "secretIdentity", "Ljava/lang/String;");
    if(secretIdentityField == NULL)
    {
        return NULL;
    }

    onlineField = cache.findField(nodeStatusClass, "online", "Z");
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
 * Signature: (J)Lcom/zerotier/sdk/VirtualNetworkConfig;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_networkConfig(
    JNIEnv *env, jobject obj, jlong id, jlong nwid)
{
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    ZT1_VirtualNetworkConfig *vnetConfig = ZT1_Node_networkConfig(node, nwid);
    
    jobject vnetConfigObject = newNetworkConfig(env, *vnetConfig);

    ZT1_Node_freeQueryResult(node, vnetConfig);

    return vnetConfigObject;
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    version
 * Signature: (J)Lcom/zerotier/sdk/Version;
 */
JNIEXPORT jobject JNICALL Java_com_zerotier_sdk_Node_version(
    JNIEnv *env, jobject obj)
{
    int major = 0;
    int minor = 0;
    int revision = 0;
    unsigned long featureFlags = 0;

    ZT1_version(&major, &minor, &revision, &featureFlags);

    return newVersion(env, major, minor, revision, featureFlags);
}

/*
 * Class:     com_zerotier_sdk_Node
 * Method:    peers
 * Signature: (J)[Lcom/zerotier/sdk/Peer;
 */
JNIEXPORT jobjectArray JNICALL Java_com_zerotier_sdk_Node_peers(
    JNIEnv *env, jobject obj, jlong id)
{
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    ZT1_PeerList *peerList = ZT1_Node_peers(node);
    
    if(peerList == NULL)
    {
        LOGE("ZT1_Node_peers returned NULL");
        return NULL;
    }

    int peerCount = peerList->peerCount * 100;
    LOGV("Ensure Local Capacity: %d", peerCount);
    if(env->EnsureLocalCapacity(peerCount))
    {
        LOGE("EnsureLocalCapacity failed!!");
        ZT1_Node_freeQueryResult(node, peerList);
        return NULL;
    }

    jclass peerClass = cache.findClass("com/zerotier/sdk/Peer");
    if(env->ExceptionCheck() || peerClass == NULL)
    {
        LOGE("Error finding Peer class");
        ZT1_Node_freeQueryResult(node, peerList);
        return NULL;
    }

    jobjectArray peerArrayObj = env->NewObjectArray(
        peerList->peerCount, peerClass, NULL);

    if(env->ExceptionCheck() || peerArrayObj == NULL)
    {
        LOGE("Error creating Peer[] array");
        ZT1_Node_freeQueryResult(node, peerList);
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
    }

    ZT1_Node_freeQueryResult(node, peerList);
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
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    ZT1_VirtualNetworkList *networkList = ZT1_Node_networks(node);
    if(networkList == NULL)
    {
        return NULL;
    }

    jclass vnetConfigClass = cache.findClass("com/zerotier/sdk/VirtualNetworkConfig");
    if(env->ExceptionCheck() || vnetConfigClass == NULL)
    {
        LOGE("Error finding VirtualNetworkConfig class");
        ZT1_Node_freeQueryResult(node, networkList);
        return NULL;
    }

    jobjectArray networkListObject = env->NewObjectArray(
        networkList->networkCount, vnetConfigClass, NULL);
    if(env->ExceptionCheck() || networkListObject == NULL)
    {
        LOGE("Error creating VirtualNetworkConfig[] array");
        ZT1_Node_freeQueryResult(node, networkList);
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
    }

    ZT1_Node_freeQueryResult(node, networkList);

    return networkListObject;
}

#ifdef __cplusplus
} // extern "C"
#endif