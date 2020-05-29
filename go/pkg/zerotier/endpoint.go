package zerotier

// #include "../../native/GoGlue.h"
// static inline const ZT_Fingerprint *_getFP(const ZT_Endpoint *ep) { return &(ep->value.fp); }
// static inline uint64_t _getAddress(const ZT_Endpoint *ep) { return ep->value.fp.address; }
// static inline uint64_t _getMAC(const ZT_Endpoint *ep) { return ep->value.mac; }
// static inline const struct sockaddr_storage *_getSS(const ZT_Endpoint *ep) { return &(ep->value.ss); }
import "C"

import (
	"encoding/json"
	"fmt"
	"unsafe"
)

const (
	EndpointTypeNil        = C.ZT_ENDPOINT_TYPE_NIL
	EndpointTypeZeroTier   = C.ZT_ENDPOINT_TYPE_ZEROTIER
	EndpointTypeEthernet   = C.ZT_ENDPOINT_TYPE_ETHERNET
	EndpointTypeWifiDirect = C.ZT_ENDPOINT_TYPE_WIFI_DIRECT
	EndpointTypeBluetooth  = C.ZT_ENDPOINT_TYPE_BLUETOOTH
	EndpointTypeIp         = C.ZT_ENDPOINT_TYPE_IP
	EndpointTypeIpUdp      = C.ZT_ENDPOINT_TYPE_IP_UDP
	EndpointTypeIpTcp      = C.ZT_ENDPOINT_TYPE_IP_TCP
	EndpointTypeIpHttp2    = C.ZT_ENDPOINT_TYPE_IP_HTTP2
)

type Endpoint struct {
	cep C.ZT_Endpoint
}

// Type returns this endpoint's type.
func (ep *Endpoint) Type() int {
	return int(ep.cep._type)
}

// InetAddress gets this Endpoint as an InetAddress or nil if its type is not addressed by one.
func (ep *Endpoint) InetAddress() *InetAddress {
	switch ep.cep._type {
	case EndpointTypeIp, EndpointTypeIpUdp, EndpointTypeIpTcp, EndpointTypeIpHttp2:
		ua := sockaddrStorageToUDPAddr(C._getSS(&ep.cep))
		return &InetAddress{IP: ua.IP, Port: ua.Port}
	}
	return nil
}

// Address returns a ZeroTier address if this is a ZeroTier endpoint or a zero address otherwise.
func (ep *Endpoint) Address() Address {
	switch ep.cep._type {
	case EndpointTypeZeroTier:
		return Address(C._getAddress(&ep.cep))
	}
	return Address(0)
}

// Fingerprint returns a fingerprint if this is a ZeroTier endpoint or nil otherwise.
func (ep *Endpoint) Fingerprint() *Fingerprint {
	switch ep.cep._type {
	case EndpointTypeZeroTier:
		cfp := C._getFP(&ep.cep)
		fp := Fingerprint{Address: Address(cfp.address), Hash: C.GoBytes(unsafe.Pointer(&cfp.hash[0]), 48)}
		if allZero(fp.Hash) {
			fp.Hash = nil
		}
		return &fp
	}
	return nil
}

// MAC returns a MAC address if this is an Ethernet type endpoint or a zero address otherwise.
func (ep *Endpoint) MAC() MAC {
	switch ep.cep._type {
	case EndpointTypeEthernet, EndpointTypeWifiDirect, EndpointTypeBluetooth:
		return MAC(C._getMAC(&ep.cep))
	}
	return MAC(0)
}

func (ep *Endpoint) String() string {
	switch ep.cep._type {
	case EndpointTypeZeroTier:
		return fmt.Sprintf("%d/%s", ep.Type(), ep.Fingerprint().String())
	case EndpointTypeEthernet, EndpointTypeWifiDirect, EndpointTypeBluetooth:
		return fmt.Sprintf("%d/%s", ep.Type(), ep.MAC().String())
	case EndpointTypeIp, EndpointTypeIpUdp, EndpointTypeIpTcp, EndpointTypeIpHttp2:
		return fmt.Sprintf("%d/%s", ep.Type(), ep.InetAddress().String())
	}
	return fmt.Sprintf("%d", ep.Type())
}

func (ep *Endpoint) MarshalJSON() ([]byte, error) {
	s := ep.String()
	return json.Marshal(&s)
}

func (ep *Endpoint) UnmarshalJSON(j []byte) error {
	// TODO
	return nil
}
