package zerotier

import (
	"encoding/binary"
	"errors"
)

// Locator objects are signed collections of physical or virtual endpoints for a node.
type Locator []byte

var (
	ErrInvalidLocator = errors.New("invalid marshaled locator object")
)

func (l Locator) Timestamp() int64 {
	if len(l) >= 8 {
		return int64(binary.BigEndian.Uint64(l))
	}
	return 0
}

// Endpoints obtains the endpoints described by this locator.
func (l Locator) Endpoints() (eps []Endpoint,err error) {
	if len(l) <= (8 + 2) {
		err = ErrInvalidLocator
		return
	}

	endpointCount := int(binary.BigEndian.Uint16(l[8:10]))
	eps = make([]Endpoint,endpointCount)
	p := 10
	for e:=0;e<endpointCount;e++ {
		if p >= len(l) {
			err = ErrInvalidLocator
			return
		}
		var elen int
		elen, err = eps[e].unmarshalZT(l[p:])
		if err != nil {
			return
		}
		p += elen
	}

	return
}
