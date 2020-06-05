package zerotier

// #include "../../serviceiocore/GoGlue.h"
// static inline const ZT_Fingerprint *_getFP(const ZT_Endpoint *ep) { return &(ep->value.fp); }
// static inline uint64_t _getAddress(const ZT_Endpoint *ep) { return ep->value.fp.address; }
// static inline uint64_t _getMAC(const ZT_Endpoint *ep) { return ep->value.mac; }
// static inline const struct sockaddr_storage *_getSS(const ZT_Endpoint *ep) { return &(ep->value.ss); }
// static inline void _setSS(ZT_Endpoint *ep,const void *ss) { memcpy(&(ep->value.ss),ss,sizeof(struct sockaddr_storage)); }
import "C"

import (
	"encoding/json"
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
	EndpointTypeIpHttp     = C.ZT_ENDPOINT_TYPE_IP_HTTP
)

type Endpoint struct {
	cep C.ZT_Endpoint
}

// NewEndpointFromString constructs a new endpoint from an InetAddress or Endpoint string.
// This will auto detect whether this is a plain InetAddress or an Endpoint in string
// format. If the former it's created as a ZT_ENDPOINT_TYPE_IP_UDP endpoint.
func NewEndpointFromString(s string) (*Endpoint, error) {
	if len(s) == 0 {
		var ep Endpoint
		ep.cep._type = C.ZT_ENDPOINT_TYPE_NIL
		return &ep, nil
	}
	var ep Endpoint
	cs := C.CString(s)
	defer C.free(unsafe.Pointer(cs))
	if C.ZT_Endpoint_fromString(&ep.cep, cs) != 0 {
		return nil, ErrInvalidParameter
	}
	return &ep, nil
}

func NewEndpointFromInetAddress(addr *InetAddress) (*Endpoint, error) {
	var ep Endpoint
	var ss C.struct_sockaddr_storage
	if !makeSockaddrStorage(addr.IP, addr.Port, &ss) {
		return nil, ErrInvalidParameter
	}
	ep.cep._type = C.ZT_ENDPOINT_TYPE_IP_UDP
	C._setSS(&ep.cep, unsafe.Pointer(&ss))
	return &ep, nil
}

// Type returns this endpoint's type.
func (ep *Endpoint) Type() int {
	return int(ep.cep._type)
}

// InetAddress gets this Endpoint as an InetAddress or nil if its type is not addressed by one.
func (ep *Endpoint) InetAddress() *InetAddress {
	switch ep.cep._type {
	case EndpointTypeIp, EndpointTypeIpUdp, EndpointTypeIpTcp, EndpointTypeIpHttp:
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
	var buf [4096]byte
	cs := C.ZT_Endpoint_toString(&ep.cep, (*C.char)(unsafe.Pointer(&buf[0])), 4096)
	if cs == nil {
		return "0"
	}
	return C.GoString(cs)
}

func (ep *Endpoint) MarshalJSON() ([]byte, error) {
	s := ep.String()
	return json.Marshal(&s)
}

func (ep *Endpoint) UnmarshalJSON(j []byte) error {
	var s string
	err := json.Unmarshal(j, &s)
	if err != nil {
		return err
	}
	ep2, err := NewEndpointFromString(s)
	if err != nil {
		return err
	}
	*ep = *ep2
	return nil
}

func (ep *Endpoint) setFromCEndpoint(cp *C.ZT_Endpoint) {
	ep.cep = *cp
}
