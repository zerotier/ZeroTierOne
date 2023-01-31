//
// Created by Brenton Bostick on 1/18/23.
//

#ifndef ZEROTIERANDROID_JNICACHE_H
#define ZEROTIERANDROID_JNICACHE_H

#include <jni.h>


//
// Classes
//

extern jclass ArrayList_class;
extern jclass DataStoreGetListener_class;
extern jclass DataStorePutListener_class;
extern jclass EventListener_class;
extern jclass Event_class;
extern jclass Inet4Address_class;
extern jclass Inet6Address_class;
extern jclass InetAddress_class;
extern jclass InetSocketAddress_class;
extern jclass NodeStatus_class;
extern jclass Node_class;
extern jclass PacketSender_class;
extern jclass PathChecker_class;
extern jclass PeerPhysicalPath_class;
extern jclass PeerRole_class;
extern jclass Peer_class;
extern jclass ResultCode_class;
extern jclass Version_class;
extern jclass VirtualNetworkConfigListener_class;
extern jclass VirtualNetworkConfigOperation_class;
extern jclass VirtualNetworkConfig_class;
extern jclass VirtualNetworkDNS_class;
extern jclass VirtualNetworkFrameListener_class;
extern jclass VirtualNetworkRoute_class;
extern jclass VirtualNetworkStatus_class;
extern jclass VirtualNetworkType_class;

//
// Instance methods
//

extern jmethodID ArrayList_add_method;
extern jmethodID ArrayList_ctor;
extern jmethodID DataStoreGetListener_onDataStoreGet_method;
extern jmethodID DataStorePutListener_onDataStorePut_method;
extern jmethodID DataStorePutListener_onDelete_method;
extern jmethodID EventListener_onEvent_method;
extern jmethodID EventListener_onTrace_method;
extern jmethodID InetAddress_getAddress_method;
extern jmethodID InetSocketAddress_ctor;
extern jmethodID InetSocketAddress_getAddress_method;
extern jmethodID InetSocketAddress_getPort_method;
extern jmethodID NodeStatus_ctor;
extern jmethodID PacketSender_onSendPacketRequested_method;
extern jmethodID PathChecker_onPathCheck_method;
extern jmethodID PathChecker_onPathLookup_method;
extern jmethodID PeerPhysicalPath_ctor;
extern jmethodID Peer_ctor;
extern jmethodID Version_ctor;
extern jmethodID VirtualNetworkConfigListener_onNetworkConfigurationUpdated_method;
extern jmethodID VirtualNetworkConfig_ctor;
extern jmethodID VirtualNetworkDNS_ctor;
extern jmethodID VirtualNetworkFrameListener_onVirtualNetworkFrame_method;
extern jmethodID VirtualNetworkRoute_ctor;

//
// Static methods
//

extern jmethodID Event_fromInt_method;
extern jmethodID InetAddress_getByAddress_method;
extern jmethodID PeerRole_fromInt_method;
extern jmethodID ResultCode_fromInt_method;
extern jmethodID VirtualNetworkConfigOperation_fromInt_method;
extern jmethodID VirtualNetworkStatus_fromInt_method;
extern jmethodID VirtualNetworkType_fromInt_method;

//
// Instance fields
//

extern jfieldID NodeStatus_address_field;
extern jfieldID NodeStatus_online_field;
extern jfieldID NodeStatus_publicIdentity_field;
extern jfieldID NodeStatus_secretIdentity_field;
extern jfieldID Node_configListener_field;
extern jfieldID Node_eventListener_field;
extern jfieldID Node_frameListener_field;
extern jfieldID Node_getListener_field;
extern jfieldID Node_pathChecker_field;
extern jfieldID Node_putListener_field;
extern jfieldID Node_sender_field;
extern jfieldID PeerPhysicalPath_address_field;
extern jfieldID PeerPhysicalPath_lastReceive_field;
extern jfieldID PeerPhysicalPath_lastSend_field;
extern jfieldID PeerPhysicalPath_preferred_field;
extern jfieldID Peer_address_field;
extern jfieldID Peer_latency_field;
extern jfieldID Peer_paths_field;
extern jfieldID Peer_role_field;
extern jfieldID Peer_versionMajor_field;
extern jfieldID Peer_versionMinor_field;
extern jfieldID Peer_versionRev_field;
extern jfieldID Version_major_field;
extern jfieldID Version_minor_field;
extern jfieldID Version_revision_field;
extern jfieldID VirtualNetworkConfig_assignedAddresses_field;
extern jfieldID VirtualNetworkConfig_bridge_field;
extern jfieldID VirtualNetworkConfig_broadcastEnabled_field;
extern jfieldID VirtualNetworkConfig_dhcp_field;
extern jfieldID VirtualNetworkConfig_dns_field;
extern jfieldID VirtualNetworkConfig_enabled_field;
extern jfieldID VirtualNetworkConfig_mac_field;
extern jfieldID VirtualNetworkConfig_mtu_field;
extern jfieldID VirtualNetworkConfig_name_field;
extern jfieldID VirtualNetworkConfig_nwid_field;
extern jfieldID VirtualNetworkConfig_portError_field;
extern jfieldID VirtualNetworkConfig_routes_field;
extern jfieldID VirtualNetworkConfig_status_field;
extern jfieldID VirtualNetworkConfig_type_field;
extern jfieldID VirtualNetworkDNS_domain_field;
extern jfieldID VirtualNetworkDNS_servers_field;
extern jfieldID VirtualNetworkRoute_flags_field;
extern jfieldID VirtualNetworkRoute_metric_field;
extern jfieldID VirtualNetworkRoute_target_field;
extern jfieldID VirtualNetworkRoute_via_field;

//
// Enums
//

extern jobject ResultCode_RESULT_FATAL_ERROR_INTERNAL_enum;
extern jobject ResultCode_RESULT_OK_enum;

void setupJNICache(JavaVM *vm);
void teardownJNICache(JavaVM *vm);

#endif // ZEROTIERANDROID_JNICACHE_H
