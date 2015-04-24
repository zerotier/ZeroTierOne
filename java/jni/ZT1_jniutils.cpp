#include "ZT1_jniutils.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    return NULL;
}

jobject appendItemToArrayList(JNIEnv *env, jobject object)
{
    return NULL;
}

jobject newIPV6Address(JNIEnv *env, char *addr)
{
    return NULL;
}

jobject newIPV4Address(JNIEnv *env, char *addr)
{
    return NULL;
}

jobject newMulticastGroup(JNIEnv *env, const ZT1_MulticastGroup &mc)
{
    return NULL;
}

#ifdef __cplusplus
}
#endif