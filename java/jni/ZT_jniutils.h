#ifndef ZT_jniutils_h_
#define ZT_jniutils_h_
#include <stdio.h>
#include <jni.h>
#include <ZeroTierOne.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_TAG "ZeroTierOneJNI"

#if __ANDROID__
#include <android/log.h>
#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#else
#define LOGV(...) fprintf(stdout, __VA_ARGS__)
#define LOGI(...) fprintf(stdout, __VA_ARGS__)
#define LOGD(...) fprintf(stdout, __VA_ARGS__)
#define LOGE(...) fprintf(stdout, __VA_ARGS__)
#endif

jobject createResultObject(JNIEnv *env, ZT_ResultCode code);
jobject createVirtualNetworkStatus(JNIEnv *env, ZT_VirtualNetworkStatus status);
jobject createVirtualNetworkType(JNIEnv *env, ZT_VirtualNetworkType type);
jobject createEvent(JNIEnv *env, ZT_Event event);
jobject createPeerRole(JNIEnv *env, ZT_PeerRole role);
jobject createVirtualNetworkConfigOperation(JNIEnv *env, ZT_VirtualNetworkConfigOperation op);

jobject newInetSocketAddress(JNIEnv *env, const sockaddr_storage &addr);
jobject newInetAddress(JNIEnv *env, const sockaddr_storage &addr);

jobject newMulticastGroup(JNIEnv *env, const ZT_MulticastGroup &mc);

jobject newPeer(JNIEnv *env, const ZT_Peer &peer);
jobject newPeerPhysicalPath(JNIEnv *env, const ZT_PeerPhysicalPath &ppp);

jobject newNetworkConfig(JNIEnv *env, const ZT_VirtualNetworkConfig &config);

jobject newVersion(JNIEnv *env, int major, int minor, int rev, long featureFlags);

#ifdef __cplusplus
}
#endif

#endif