package attic

import (
	"encoding/binary"
	"errors"
)

// Locator objects are signed collections of physical or virtual endpoints for a node.
type Locator []byte

var (
	ErrInvalidLocator = errors.New("invalid marshaled locator object")
)

// Timestamp returns this locator's timestamp in milliseconds since epoch.
func (l Locator) Timestamp() int64 {
	if len(l) >= 8 {
		return int64(binary.BigEndian.Uint64(l[0:8]))
	}
	return 0
}

// Nil returns true if this is a nil/empty locator.
func (l Locator) Nil() bool {
	return len(l) < 8 || int64(binary.BigEndian.Uint64(l[0:8])) <= 0
}

// Endpoints obtains the endpoints described by this locator.
func (l Locator) Endpoints() (eps []Endpoint, err error) {
	if len(l) < 8 {
		err = ErrInvalidLocator
		return
	}
	if int64(binary.BigEndian.Uint64(l[0:8])) > 0 {
		if len(l) < 10 {
			err = ErrInvalidLocator
			return
		}
		endpointCount := int(binary.BigEndian.Uint16(l[8:10]))
		eps = make([]Endpoint, endpointCount)
		p := 10
		for e := 0; e < endpointCount; e++ {
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
	}
	return
}
