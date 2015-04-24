#ifndef ZT1_jniutils_h_
#define ZT1_jniutils_h_
#include <jni.h>
#include <ZeroTierOne.h>

#ifdef __cplusplus
extern "C" {
#endif

jobject createResultObject(JNIEnv *env, ZT1_ResultCode code);
jobject createVirtualNetworkStatus(JNIEnv *env, ZT1_VirtualNetworkStatus status);
jobject createVirtualNetworkType(JNIEnv *env, ZT1_VirtualNetworkType type);
jobject createEvent(JNIEnv *env, ZT1_Event event);

jobject newArrayList(JNIEnv *env);
bool appendItemToArrayList(JNIEnv *env, jobject array, jobject object);

jobject newInetAddress(JNIEnv *env, const sockaddr_storage &addr);

jobject newMulticastGroup(JNIEnv *env, const ZT1_MulticastGroup &mc);
#ifdef __cplusplus
}
#endif

#endif