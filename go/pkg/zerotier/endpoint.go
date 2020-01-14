package zerotier

import (
	"encoding/binary"
	"errors"
)

// Endpoint types are the same as the enum values in Endpoint.hpp in the core.
const (
	EndpointTypeNil          = 0
	EndpointTypeInetAddr     = 1
	EndpointTypeDnsName      = 2
	EndpointTypeZeroTier     = 3
	EndpointTypeUrl          = 4
	EndpointTypeEthernet     = 5
	EndpointTypeUnrecognized = 255
)

type Endpoint struct {
	Type          int
	value, value2 interface{}
}

var (
	ErrInvalidEndpoint = errors.New("invalid marshaled endpoint object")
)

func (ep *Endpoint) unmarshalZT(b []byte) (int, error) {
	if len(b) == 0 {
		return 0, ErrInvalidEndpoint
	}
	switch b[0] {
	case EndpointTypeNil:
		*ep = Endpoint{Type: EndpointTypeNil}
		return 1, nil
	case EndpointTypeInetAddr:
		ina := new(InetAddress)
		inlen, err := ina.unmarshalZT(b[1:])
		if err != nil {
			return 0, err
		}
		*ep = Endpoint{
			Type:  EndpointTypeInetAddr,
			value: ina,
		}
		return 1 + inlen, nil
	case EndpointTypeDnsName:
		zeroAt := 1
		for i := 1; i < len(b); i++ {
			if b[i] == 0 {
				zeroAt = i
				break
			}
		}
		if zeroAt == 1 || (1 + zeroAt + 3) > len(b) {
			return 0, ErrInvalidEndpoint
		}
		port := binary.BigEndian.Uint16(b[zeroAt+1:zeroAt+3])
		*ep = Endpoint{
			Type:   EndpointTypeDnsName,
			value:  string(b[1:zeroAt]),
			value2: &port,
		}
		return zeroAt + 3, nil
	case EndpointTypeZeroTier:
		if len(b) != 54 {
			return 0, ErrInvalidEndpoint
		}
		a, err := NewAddressFromBytes(b[1:6])
		if err != nil {
			return 0, err
		}
		*ep = Endpoint{
			Type:   EndpointTypeZeroTier,
			value:  a,
			value2: append(make([]byte, 0, 48), b[6:54]...),
		}
		return 54, nil
	case EndpointTypeUrl:
		zeroAt := 1
		for i := 1; i < len(b); i++ {
			if b[i] == 0 {
				zeroAt = i
				break
			}
		}
		if zeroAt == 1 {
			return 0, ErrInvalidEndpoint
		}
		*ep = Endpoint{
			Type:  EndpointTypeUrl,
			value: string(b[1:zeroAt]),
		}
		return zeroAt + 2, nil
	case EndpointTypeEthernet:
		if len(b) != 7 {
			return 0, ErrInvalidEndpoint
		}
		m, err := NewMACFromBytes(b[1:7])
		if err != nil {
			return 0, err
		}
		*ep = Endpoint{
			Type: EndpointTypeEthernet,
			value: m,
		}
		return 7, nil
	default:
		if len(b) < 2 {
			return 0, ErrInvalidEndpoint
		}
		*ep = Endpoint{Type: EndpointTypeUnrecognized}
		return 1 + int(b[1]), nil
	}
}
