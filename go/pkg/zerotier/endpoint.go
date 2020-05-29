package zerotier

// #include "../../native/GoGlue.h"
import "C"

import (
	"encoding/json"
	"fmt"
	"strconv"
	"strings"
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
	return ep.cep._type
}

// InetAddress gets this Endpoint as an InetAddress or nil if its type is not addressed by one.
func (ep *Endpoint) InetAddress() *InetAddress {
	switch ep.cep._type {
	case EndpointTypeIp, EndpointTypeIpUdp, EndpointTypeIpTcp, EndpointTypeIpHttp2:
		ua := sockaddrStorageToUDPAddr(&(ep.cep.a.ss))
		return &InetAddress{IP: ua.IP, Port: ua.Port}
	}
	return nil
}

func (ep *Endpoint) String() string {
	switch ep.cep._type {
	case EndpointTypeZeroTier:
		fp := Fingerprint{Address: Address(ep.cep.a.fp.address), Hash: *((*[48]byte)(unsafe.Pointer(&ep.cep.a.fp.hash[0])))}
		return fmt.Sprintf("%d/%s", ep.Type(), fp.String())
	case EndpointTypeEthernet, EndpointTypeWifiDirect, EndpointTypeBluetooth:
		return fmt.Sprintf("%d/%s", ep.Type(), MAC(ep.cep.a.mac).String())
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
	var s string
	err := json.Unmarshal(j, &s)
	if err != nil {
		return err
	}

	slashIdx := strings.IndexRune(s, '/')
	if slashIdx < 0 {
		ep.cep._type = C.uint(strconv.ParseUint(s, 10, 32))
	} else if slashIdx == 0 || slashIdx >= (len(s)-1) {
		return ErrInvalidParameter
	} else {
		ep.cep._type = C.uint(strconv.ParseUint(s[0:slashIdx], 10, 32))
		s = s[slashIdx+1:]
	}

	switch ep.cep._type {
	case EndpointTypeNil:
		return nil
	case EndpointTypeZeroTier:
		fp, err := NewFingerprintFromString(s)
		if err != nil {
			return err
		}
		ep.cep.a.fp.address = C.uint64_t(fp.Address)
		copy(((*[48]byte)(unsafe.Pointer(&ep.cep.a.fp.hash[0])))[:], fp.Hash[:])
		return nil
	}
	return ErrInvalidParameter
}
