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
jobject createPeerRole(JNIEnv *env, ZT1_PeerRole role);
jobject createVirtualNetworkConfigOperation(JNIEnv *env, ZT1_VirtualNetworkConfigOperation op);

jobject newArrayList(JNIEnv *env);
bool appendItemToArrayList(JNIEnv *env, jobject array, jobject object);

jobject newInetAddress(JNIEnv *env, const sockaddr_storage &addr);

jobject newMulticastGroup(JNIEnv *env, const ZT1_MulticastGroup &mc);

jobject newPeer(JNIEnv *env, const ZT1_Peer &peer);
jobject newPeerPhysicalPath(JNIEnv *env, const ZT1_PeerPhysicalPath &ppp);

jobject newNetworkConfig(JNIEnv *env, const ZT1_VirtualNetworkConfig &config);

#ifdef __cplusplus
}
#endif

#endif