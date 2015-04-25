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

#include <ZeroTierOne.h>

#include <map>
#include <string>
#include <assert.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

namespace {
    struct JniRef
    {
        JniRef()
            : env(NULL)
            , node(NULL)
            , dataStoreGetListener(NULL)
            , dataStorePutListener(NULL)
            , packetSender(NULL)
            , frameListener(NULL)
            , configListener(NULL)
        {}
        uint64_t id;

        JNIEnv *env;

        ZT1_Node *node;

        jobject dataStoreGetListener;
        jobject dataStorePutListener;
        jobject packetSender;
        jobject frameListener;
        jobject configListener;
    };


    int VirtualNetworkConfigFunctionCallback(ZT1_Node *node,void *userData,uint64_t nwid,enum ZT1_VirtualNetworkConfigOperation operation, const ZT1_VirtualNetworkConfig *config)
    {
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);

        JNIEnv *env = ref->env;

        static jclass configListenerClass = NULL;
        static jmethodID callbackMethod = NULL;

        if(configListenerClass == NULL)
        {
            configListenerClass = env->GetObjectClass(ref->configListener);
            if(configListenerClass == NULL)
            {
                return -1;
            }
        }

        if(callbackMethod == NULL)
        {
            callbackMethod = env->GetMethodID(configListenerClass,
                "onNetworkConfigurationUpdated",
                "(JLcom/zerotierone/sdk/VirtualNetworkConfigOperation;Lcom/zerotierone/sdk/VirtualNetworkConfig;)I");
            if(callbackMethod == NULL)
            {
                return -2;
            }
        }

        jobject operationObject = createVirtualNetworkConfigOperation(env, operation);
        if(operationObject == NULL)
        {
            return -3;
        }

        jobject networkConfigObject = newNetworkConfig(env, *config);
        if(networkConfigObject == NULL)
        {
            return -4;
        }

        return env->CallIntMethod(ref->configListener, callbackMethod, (jlong)nwid, operationObject, networkConfigObject);
    }

    void VirtualNetworkFrameFunctionCallback(ZT1_Node *node,void *userData,uint64_t,uint64_t,uint64_t,unsigned int,unsigned int,const void *,unsigned int)
    {
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);

        JNIEnv *env = ref->env;
    }

    void EventCallback(ZT1_Node *node,void *userData,enum ZT1_Event,const void *)
    {
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);

        JNIEnv *env = ref->env;
    }

    long DataStoreGetFunction(ZT1_Node *node,void *userData,const char *,void *,unsigned long,unsigned long,unsigned long *)
    {
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);

        JNIEnv *env = ref->env;

        return 0;
    }

    int DataStorePutFunction(ZT1_Node *node,void *userData,const char *,const void *,unsigned long,int)
    {
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);

        JNIEnv *env = ref->env;

        return 0;
    }

    int WirePacketSendFunction(ZT1_Node *node,void *userData,const struct sockaddr_storage *,unsigned int,const void *,unsigned int)
    {
        JniRef *ref = (JniRef*)userData;
        assert(ref->node == node);

        JNIEnv *env = ref->env;

        return 0;
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

/*
 * Class:     com_zerotierone_sdk_Node
 * Method:    node_init
 * Signature: (J)Lcom/zerotierone/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_node_1init(
    JNIEnv *env, jobject obj, jlong now)
{
    jobject resultObject = createResultObject(env, ZT1_RESULT_OK);

    ZT1_Node *node;
    JniRef *ref = new JniRef;

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

    
    ref->id = (uint64_t)now;
    ref->env = env;
    ref->node = node;

    jclass cls = env->GetObjectClass(obj);
    jfieldID fid = env->GetFieldID(
        cls, "getListener", "Lcom/zerotierone/sdk/DataStoreGetListener;");

    if(fid == NULL)
    {
        return NULL; // exception already thrown
    }

    ref->dataStoreGetListener = env->GetObjectField(obj, fid);
    if(ref->dataStoreGetListener == NULL)
    {
        return NULL;
    }

    fid = env->GetFieldID(
        cls, "putListener", "Lcom/zerotierone/sdk/DataStorePutLisetner;");

    if(fid == NULL)
    {
        return NULL; // exception already thrown
    }

    ref->dataStorePutListener = env->GetObjectField(obj, fid);
    if(ref->dataStorePutListener == NULL)
    {
        return NULL;
    }

    fid = env->GetFieldID(
        cls, "sender", "Lcom/zerotierone/sdk/PacketSender;");
    if(fid == NULL)
    {
        return NULL; // exception already thrown
    }

    ref->packetSender = env->GetObjectField(obj, fid);
    if(ref->packetSender == NULL)
    {
        return NULL;
    }

    fid = env->GetFieldID(
        cls, "frameListener", "Lcom/zerotierone/sdk/VirtualNetworkFrameListener;");
    if(fid == NULL)
    {
        return NULL; // exception already thrown
    }

    ref->frameListener = env->GetObjectField(obj, fid);
    if(ref->frameListener = NULL)
    {
        return NULL;
    }

    fid = env->GetFieldID(
        cls, "configListener", "Lcom/zerotierone/sdk/VirtualNetworkConfigListener;");
    if(fid == NULL)
    {
        return NULL; // exception already thrown
    }

    ref->configListener = env->GetObjectField(obj, fid);
    if(ref->configListener == NULL)
    {
        return NULL;
    }

    nodeMap.insert(std::make_pair(ref->id, ref));

    return resultObject;
}

/*
 * Class:     com_zerotierone_sdk_Node
 * Method:    node_delete
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_zerotierone_sdk_Node_node_1delete(
    JNIEnv *env, jobject obj, jlong id)
{
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
}

/*
 * Class:     com_zerotierone_sdk_Node
 * Method:    processVirtualNetworkFrame
 * Signature: (JJJJJII[B[J)Lcom/zerotierone/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_processVirtualNetworkFrame(
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
 * Class:     com_zerotierone_sdk_Node
 * Method:    processWirePacket
 * Signature: (JJLjava/net/InetAddress;I[B[J)Lcom/zerotierone/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_processWirePacket(
    JNIEnv *env, jobject obj, 
    jlong id,
    jlong in_now, 
    jobject in_remoteAddress,
    jint in_linkDesparation,
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
    unsigned int linkDesparation = (unsigned int)in_linkDesparation;

    // get the java.net.InetAddress class and getAddress() method
    jclass inetAddressClass = env->FindClass("java/net/InetAddress");
    if(inetAddressClass == NULL)
    {
        // can't find java.net.InetAddress
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    jmethodID getAddressMethod = env->GetMethodID(
        inetAddressClass, "getAddress", "()[B");
    if(getAddressMethod == NULL)
    {
        // cant find InetAddress.getAddres()
        return createResultObject(env, ZT1_RESULT_FATAL_ERROR_INTERNAL);
    }

    // Call InetAddress.getAddress()
    jbyteArray addressArray = (jbyteArray)env->CallObjectMethod(in_remoteAddress, getAddressMethod);
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
    else if(addrSize = 4)
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
        linkDesparation,
        packetData,
        packetLength,
        &nextBackgroundTaskDeadline);

    jlong *outDeadline = env->GetLongArrayElements(out_nextBackgroundTaskDeadline, NULL);
    outDeadline[0] = (jlong)nextBackgroundTaskDeadline;
    env->ReleaseLongArrayElements(out_nextBackgroundTaskDeadline, outDeadline, 0);

    env->ReleaseByteArrayElements(addressArray, addr, 0);
    env->ReleaseByteArrayElements(in_packetData, packetData, 0);

    return createResultObject(env, ZT1_RESULT_OK);
}

/*
 * Class:     com_zerotierone_sdk_Node
 * Method:    processBackgroundTasks
 * Signature: (JJ[J)Lcom/zerotierone/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_processBackgroundTasks(
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
 * Class:     com_zerotierone_sdk_Node
 * Method:    join
 * Signature: (JJ)Lcom/zerotierone/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_join(
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
 * Class:     com_zerotierone_sdk_Node
 * Method:    leave
 * Signature: (JJ)Lcom/zerotierone/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_leave(
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
 * Class:     com_zerotierone_sdk_Node
 * Method:    multicastSubscribe
 * Signature: (JJJJ)Lcom/zerotierone/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_multicastSubscribe(
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
    uint64_t multicastAdi = (uint64_t)in_multicastAdi;

    ZT1_ResultCode rc = ZT1_Node_multicastSubscribe(
        node, nwid, multicastGroup, multicastAdi);

    return createResultObject(env, rc);
}

/*
 * Class:     com_zerotierone_sdk_Node
 * Method:    multicastUnsubscribe
 * Signature: (JJJJ)Lcom/zerotierone/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_multicastUnsubscribe(
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
    uint64_t multicastAdi = (uint64_t)in_multicastAdi;

    ZT1_ResultCode rc = ZT1_Node_multicastUnsubscribe(
        node, nwid, multicastGroup, multicastAdi);

    return createResultObject(env, rc);
}

/*
 * Class:     com_zerotierone_sdk_Node
 * Method:    address
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_com_zerotierone_sdk_Node_address(
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
 * Class:     com_zerotierone_sdk_Node
 * Method:    status
 * Signature: (J)Lcom/zerotierone/sdk/NodeStatus;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_status
   (JNIEnv *env, jobject obj, jlong id)
{
    uint64_t nodeId = (uint64_t) id;
    ZT1_Node *node = findNode(nodeId);
    if(node == NULL)
    {
        // cannot find valid node.  We should  never get here.
        return 0;
    }

    // static so we only have to look these up once
    static jclass nodeStatusClass = NULL;
    static jmethodID nodeStatusConstructor = NULL;

    // create a com.zerotierone.sdk.NodeStatus object
    if(nodeStatusClass == NULL)
    {
        nodeStatusClass = env->FindClass("com/zerotierone/sdk/NodeStatus");
        if(nodeStatusClass == NULL)
        {
            return NULL;
        }
    }
    
    if(nodeStatusConstructor == NULL)
    {
        nodeStatusConstructor = env->GetMethodID(
            nodeStatusClass, "<init>", "()V");
        if(nodeStatusConstructor == NULL)
        {
            return NULL;
        }
    }

    jobject nodeStatusObj = env->NewObject(nodeStatusClass, nodeStatusConstructor);
    if(nodeStatusObj == NULL)
    {
        return NULL;
    }

    ZT1_NodeStatus nodeStatus;
    ZT1_Node_status(node, &nodeStatus);

    static jfieldID addressField = NULL;
    static jfieldID publicIdentityField = NULL;
    static jfieldID secretIdentityField = NULL;
    static jfieldID onlineField = NULL;

    if(addressField == NULL)
    {
        addressField = env->GetFieldID(nodeStatusClass, "address", "J");
        if(addressField == NULL)
        {
            return NULL;
        }
    }

    if(publicIdentityField == NULL)
    {
        publicIdentityField = env->GetFieldID(nodeStatusClass, "publicIdentity", "Ljava/lang/String;");
        if(publicIdentityField == NULL)
        {
            return NULL;
        }
    }

    if(secretIdentityField == NULL)
    {
        secretIdentityField = env->GetFieldID(nodeStatusClass, "secretIdentity", "Ljava/lang/String;");
        if(secretIdentityField == NULL)
        {
            return NULL;
        }
    }

    if(onlineField == NULL)
    {
        onlineField = env->GetFieldID(nodeStatusClass, "online", "Z");
        if(onlineField == NULL)
        {
            return NULL;
        }
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
 * Class:     com_zerotierone_sdk_Node
 * Method:    networkConfig
 * Signature: (J)Lcom/zerotierone/sdk/VirtualNetworkConfig;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_networkConfig(
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
 * Class:     com_zerotierone_sdk_Node
 * Method:    version
 * Signature: (J)Lcom/zerotierone/sdk/Version;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_version(
    JNIEnv *env, jobject obj)
{
    // create a com.zerotierone.sdk.Version object
    jclass versionClass = env->FindClass("com/zerotierone/sdk/Version");
    if(versionClass == NULL)
    {
        return NULL;
    }

    jmethodID versionConstructor = env->GetMethodID(
        versionClass, "<init>", "()V");
    if(versionConstructor == NULL)
    {
        return NULL;
    }

    jobject versionObj = env->NewObject(versionClass, versionConstructor);
    if(versionObj == NULL)
    {
        return NULL;
    }

    int major = 0;
    int minor = 0;
    int revision = 0;
    unsigned long featureFlags = 0;

    ZT1_version(&major, &minor, &revision, &featureFlags);

    // copy data to Version object
    static jfieldID majorField = NULL;
    static jfieldID minorField = NULL;
    static jfieldID revisionField = NULL;
    static jfieldID featureFlagsField = NULL;

    if(majorField == NULL)
    {
        majorField = env->GetFieldID(versionClass, "major", "I");
        if(majorField = NULL)
        {
            return NULL;
        }
    }

    if(minorField == NULL)
    {
        minorField = env->GetFieldID(versionClass, "minor", "I");
        if(minorField == NULL)
        {
            return NULL;
        }
    }

    if(revisionField == NULL)
    {
        revisionField = env->GetFieldID(versionClass, "revision", "I");
        if(revisionField == NULL)
        {
            return NULL;
        }
    }

    if(featureFlagsField == NULL)
    {
        featureFlagsField = env->GetFieldID(versionClass, "featureFlags", "J");
        if(featureFlagsField == NULL)
        {
            return NULL;
        }
    }

    env->SetIntField(versionObj, majorField, (jint)major);
    env->SetIntField(versionObj, minorField, (jint)minor);
    env->SetIntField(versionObj, revisionField, (jint)revision);
    env->SetLongField(versionObj, featureFlagsField, (jlong)featureFlags);


    return versionObj;
}

/*
 * Class:     com_zerotierone_sdk_Node
 * Method:    peers
 * Signature: (J)Ljava/util/ArrayList;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_peers(
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
        return NULL;
    }

    jobject peerListObject = newArrayList(env);
    if(peerListObject == NULL)
    {
        ZT1_Node_freeQueryResult(node, peerList);
        return NULL;
    }

    for(unsigned int i = 0; i < peerList->peerCount; ++i)
    {
        jobject peerObj = newPeer(env, peerList->peers[i]);
        appendItemToArrayList(env, peerListObject, peerObj);
    }

    ZT1_Node_freeQueryResult(node, peerList);
    peerList = NULL;

    return peerListObject;
}

/*
 * Class:     com_zerotierone_sdk_Node
 * Method:    networks
 * Signature: (J)Ljava/util/ArrayList;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_networks(
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

    jobject networkListObject = newArrayList(env);
    if(networkListObject == NULL)
    {
        ZT1_Node_freeQueryResult(node, networkList);
        return NULL;
    }

    for(unsigned int i = 0; i < networkList->networkCount; ++i)
    {
        jobject networkObject = newNetworkConfig(env, networkList->networks[i]);
        appendItemToArrayList(env, networkListObject, networkObject);
    }

    ZT1_Node_freeQueryResult(node, networkList);

    return networkListObject;
}

#ifdef __cplusplus
} // extern "C"
#endif