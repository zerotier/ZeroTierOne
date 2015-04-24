#ifndef ZT1_jniutils_h_
#define ZT1_jniutils_h_
#include <jni.h>
#include <ZeroTierOne.h>

#ifdef __cplusplus
extern "C" {
#endif

jobject newArrayList(JNIEnv *env);
jobject appendItemToArrayList(JNIEnv *env, jobject object);

jobject newIPV6Address(JNIEnv *env, char *addr);
jobject newIPV4Address(JNIEnv *env, char *addr);

jobject newMulticastGroup(JNIEnv *env, const ZT1_MulticastGroup &mc);
#ifdef __cplusplus
}
#endif

#endif