/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */
 
#ifndef ZT_jniutils_h_
#define ZT_jniutils_h_
#include <stdio.h>
#include <jni.h>
#include <ZeroTierOne.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_TAG "ZeroTierOneJNI"

#if defined(__ANDROID__)

#include <android/log.h>

    #if !defined(NDEBUG)
        #define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
        #define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
    #else
        #define LOGV(...)
        #define LOGD(...)
    #endif

    #define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
    #define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#else
    #if !defined(NDEBUG)
        #define LOGV(...) fprintf(stdout, __VA_ARGS__)
        #define LOGD(...) fprintf(stdout, __VA_ARGS__)
    #else
        #define LOGV(...)
        #define LOGD(...)
    #endif

    #define LOGI(...) fprintf(stdout, __VA_ARGS__)
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

jobject newVersion(JNIEnv *env, int major, int minor, int rev);

jobject newVirtualNetworkRoute(JNIEnv *env, const ZT_VirtualNetworkRoute &route);

#ifdef __cplusplus
}
#endif

#endif