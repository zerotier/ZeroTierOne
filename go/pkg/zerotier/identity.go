/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package zerotier

import (
	secrand "crypto/rand"
	"crypto/sha512"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"strings"

	"golang.org/x/crypto/salsa20/salsa"

	"golang.org/x/crypto/curve25519"

	"golang.org/x/crypto/ed25519"
)

const ztIdentityGenMemory = 2097152
const ztIdentityHashCashFirstByteLessThan = 17

// IdentityTypeC25519 is a classic Curve25519/Ed25519 identity
const IdentityTypeC25519 = 0

// IdentityTypeP384 is an identity containing both NIST P-384 and Curve25519/Ed25519 key types and leveraging both when possible
const IdentityTypeP384 = 1

func computeZeroTierIdentityMemoryHardHash(publicKey []byte) []byte {
	s512 := sha512.Sum512(publicKey)

	var genmem [ztIdentityGenMemory]byte
	var s20key [32]byte
	var s20ctr [16]byte
	var s20ctri uint64
	copy(s20key[:], s512[0:32])
	copy(s20ctr[0:8], s512[32:40])
	salsa.XORKeyStream(genmem[0:64], genmem[0:64], &s20ctr, &s20key)
	s20ctri++
	for i := 64; i < ztIdentityGenMemory; i += 64 {
		binary.LittleEndian.PutUint64(s20ctr[8:16], s20ctri)
		salsa.XORKeyStream(genmem[i:i+64], genmem[i-64:i], &s20ctr, &s20key)
		s20ctri++
	}

	var tmp [8]byte
	for i := 0; i < ztIdentityGenMemory; {
		idx1 := uint(binary.BigEndian.Uint64(genmem[i:])&7) * 8
		i += 8
		idx2 := (uint(binary.BigEndian.Uint64(genmem[i:])) % uint(ztIdentityGenMemory/8)) * 8
		i += 8
		gm := genmem[idx2 : idx2+8]
		d := s512[idx1 : idx1+8]
		copy(tmp[:], gm)
		copy(gm, d)
		copy(d, tmp[:])
		binary.LittleEndian.PutUint64(s20ctr[8:16], s20ctri)
		salsa.XORKeyStream(s512[:], s512[:], &s20ctr, &s20key)
		s20ctri++
	}

	return s512[:]
}

// generateDualPair generates a key pair containing two pairs: one for curve25519 and one for ed25519.
func generateDualPair() (pub [64]byte, priv [64]byte) {
	k0pub, k0priv, _ := ed25519.GenerateKey(secrand.Reader)
	var k1pub, k1priv [32]byte
	secrand.Read(k1priv[:])
	curve25519.ScalarBaseMult(&k1pub, &k1priv)
	copy(pub[0:32], k1pub[:])
	copy(pub[32:64], k0pub[0:32])
	copy(priv[0:32], k1priv[:])
	copy(priv[32:64], k0priv[0:32])
	return
}

// Identity is precisely what it sounds like: the address and associated keys for a ZeroTier node
type Identity struct {
	// Address is this identity's 40-bit short address
	Address Address

	// Type is either IdentityTypeC25519 or IdentityTypeP384
	Type int

	// PublicKey is this identity's public key bytes
	PublicKey Blob

	privateKey []byte
}

// NewIdentity generates a new ZeroTier Identity.
// This can be a little bit time consuming due to one way proof of work requirements (usually a few hundred milliseconds).
func NewIdentity() (id Identity) {
	for {
		pub, priv := generateDualPair()
		dig := computeZeroTierIdentityMemoryHardHash(pub[:])
		if dig[0] < ztIdentityHashCashFirstByteLessThan && dig[59] != 0xff {
			addr := uint64(dig[59])
			addr <<= 8
			addr |= uint64(dig[60])
			addr <<= 8
			addr |= uint64(dig[61])
			addr <<= 8
			addr |= uint64(dig[62])
			addr <<= 8
			addr |= uint64(dig[63])
			if addr != 0 {
				id.Address = Address(addr)
				id.PublicKey = pub[:]
				id.privateKey = priv[:]
				break
			}
		}
	}
	return
}

// NewIdentityFromString generates a new identity from its string representation.
// The private key is imported as well if it is present.
func NewIdentityFromString(s string) (*Identity, error) {
	ss := strings.Split(s, ":")
	if len(ss) < 3 {
		return nil, ErrInvalidParameter
	}

	var err error
	var id Identity
	id.Address, err = NewAddressFromString(ss[0])
	if err != nil {
		return nil, err
	}

	if ss[1] == "0" {
		id.Type = 0
	} else if ss[1] == "1" {
		id.Type = 1
	} else {
		return nil, ErrUncrecognizedIdentityType
	}

	switch id.Type {
	case 0:
		id.PublicKey, err = hex.DecodeString(ss[2])
		if err != nil {
			return nil, err
		}
		if len(ss) >= 4 {
			id.privateKey, err = hex.DecodeString(ss[3])
			if err != nil {
				return nil, err
			}
		}
	case 1:

	}

	return &id, nil
}

// HasPrivate returns true if this identity has its own private portion.
func (id *Identity) HasPrivate() bool { return len(id.privateKey) > 0 }

// PrivateKeyString returns the full identity.secret if the private key is set, or an empty string if no private key is set.
func (id *Identity) PrivateKeyString() string {
	if len(id.privateKey) == 64 {
		s := fmt.Sprintf("%.10x:0:%x:%x", id.Address, id.PublicKey, id.privateKey)
		return s
	}
	return ""
}

// PublicKeyString returns the address and public key (identity.public contents).
// An empty string is returned if this identity is invalid or not initialized.
func (id *Identity) String() string {
	if len(id.PublicKey) == 64 {
		s := fmt.Sprintf("%.10x:0:%x", id.Address, id.PublicKey)
		return s
	}
	return ""
}
