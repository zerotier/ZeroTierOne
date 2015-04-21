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

#include <jni.h>

#include <ZeroTierOne.h>

#include <map>
#include <assert.h>

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


	int VirtualNetworkConfigFunctionCallback(ZT1_Node *node,void *userData,uint64_t,enum ZT1_VirtualNetworkConfigOperation,const ZT1_VirtualNetworkConfig *)
	{
		JniRef *ref = (JniRef*)userData;
		assert(ref->node == node);

		JNIEnv *env = ref->env;

		return 0;
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

	jobject createResultObject(JNIEnv *env, ZT1_ResultCode code)
	{
		// cache the class and constructor so we don't have to
		// look them up every time we need to create a java
		// ResultCode object
		static jclass resultClass = NULL;
		static jmethodID constructorId = NULL;

		jobject resultObject = NULL;

		if(resultClass == NULL)
		{
			resultClass = env->FindClass("com/zerotierone/sdk/ResultCode");
			if(resultClass == NULL)
			{
				return NULL; // exception thrown
			}
		}

		if(constructorId = NULL)
		{
			constructorId = env->GetMethodID(resultClass, "<init>", "(I)V");
			if(constructorId == NULL)
			{
				return NULL; // exception thrown
			}
		}

		resultObject = env->NewObject(resultClass, constructorId, (jlong)code);

		return resultObject;
	}
}

/*
 * Class:     com_zerotierone_sdk_Node
 * Method:    node_init
 * Signature: (J)Lcom/zerotierone/sdk/ResultCode;
 */
JNIEXPORT jobject JNICALL Java_com_zerotierone_sdk_Node_node_1init
  (JNIEnv *env, jobject obj, jlong now)
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
		cls, "getListener", "Lcom.zerotierone.sdk.DataStoreGetListener;");

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
		cls, "putListener", "Lcom.zerotierone.sdk.DataStorePutLisetner;");

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
		cls, "sender", "Lcom.zerotierone.sdk.PacketSender;");
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
		cls, "frameListener", "Lcom.zerotierone.sdk.VirtualNetworkFrameListener;");
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
		cls, "configListener", "Lcom.zerotierone.sdk.VirtualNetworkConfigListener;");
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
JNIEXPORT void JNICALL Java_com_zerotierone_sdk_Node_node_1delete
  (JNIEnv *env, jobject obj, jlong id)
{
	uint64_t nodeId = (uint64_t)id;

	NodeMap::iterator found = nodeMap.find(nodeId);
	if(found != nodeMap.end())
	{
		JniRef *ref = found->second;
		nodeMap.erase(found);
		delete ref;
		ref = NULL;
	}
}



#ifdef __cplusplus
} // extern "C"
#endif