package attic

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

// Endpoint wraps a variety of different ways of describing a node's physical network location.
type Endpoint struct {
	// Type is this endpoint's type
	Type int

	// Location is the X, Y, Z coordinate of this endpoint or 0,0,0 if unspecified.
	Location [3]int

	value, value2 interface{}
}

var (
	ErrInvalidEndpoint = errors.New("invalid marshaled endpoint object")
)

func (ep *Endpoint) unmarshalZT(b []byte) (int, error) {
	if len(b) < 7 {
		return 0, ErrInvalidEndpoint
	}
	ep.Type = int(b[0])
	ep.Location[0] = int(binary.BigEndian.Uint16(b[1:3]))
	ep.Location[1] = int(binary.BigEndian.Uint16(b[3:5]))
	ep.Location[2] = int(binary.BigEndian.Uint16(b[5:7]))
	ep.value = nil
	ep.value2 = nil
	switch ep.Type {
	case EndpointTypeNil:
		return 7, nil
	case EndpointTypeInetAddr:
		ina := new(InetAddress)
		inlen, err := ina.unmarshalZT(b[7:])
		if err != nil {
			return 0, err
		}
		ep.value = ina
		return 7 + inlen, nil
	case EndpointTypeDnsName:
		stringEnd := 0
		for i := 7; i < len(b); i++ {
			if b[i] == 0 {
				stringEnd = i + 1
				break
			}
		}
		if stringEnd == 0 || (stringEnd+2) > len(b) {
			return 0, ErrInvalidEndpoint
		}
		ep.value = string(b[7:stringEnd])
		port := binary.BigEndian.Uint16(b[stringEnd : stringEnd+2])
		ep.value2 = &port
		return stringEnd + 2, nil
	case EndpointTypeZeroTier:
		if len(b) < 60 {
			return 0, ErrInvalidEndpoint
		}
		a, err := NewAddressFromBytes(b[7:12])
		if err != nil {
			return 0, err
		}
		ep.value = a
		ep.value2 = append(make([]byte, 0, 48), b[12:60]...)
		return 60, nil
	case EndpointTypeUrl:
		stringEnd := 0
		for i := 7; i < len(b); i++ {
			if b[i] == 0 {
				stringEnd = i + 1
				break
			}
		}
		if stringEnd == 0 {
			return 0, ErrInvalidEndpoint
		}
		ep.value = string(b[7:stringEnd])
		return stringEnd, nil
	case EndpointTypeEthernet:
		if len(b) < 13 {
			return 0, ErrInvalidEndpoint
		}
		m, err := NewMACFromBytes(b[7:13])
		if err != nil {
			return 0, err
		}
		ep.value = m
		return 13, nil
	default:
		if len(b) < 8 {
			return 0, ErrInvalidEndpoint
		}
		ep.Type = EndpointTypeUnrecognized
		return 8 + int(b[1]), nil
	}
}

// InetAddress gets the address associated with this endpoint or nil if it is not of this type.
func (ep *Endpoint) InetAddress() *InetAddress {
	v, _ := ep.value.(*InetAddress)
	return v
}

// Address gets the address associated with this endpoint or nil if it is not of this type.
func (ep *Endpoint) Address() *Address {
	v, _ := ep.value.(*Address)
	return v
}

// DNSName gets the DNS name and port associated with this endpoint or an empty string and -1 if it is not of this type.
func (ep *Endpoint) DNSName() (string, int) {
	if ep.Type == EndpointTypeDnsName {
		return ep.value.(string), int(*(ep.value2.(*uint16)))
	}
	return "", -1
}

// InetAddress gets the URL assocaited with this endpoint or an empty string if it is not of this type.
func (ep *Endpoint) URL() string {
	if ep.Type == EndpointTypeUrl {
		return ep.value.(string)
	}
	return ""
}

// Ethernet gets the address associated with this endpoint or nil if it is not of this type.
func (ep *Endpoint) Ethernet() *MAC {
	v, _ := ep.value.(*MAC)
	return v
}
