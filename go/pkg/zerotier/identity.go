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

//#include "../../native/GoGlue.h"
import "C"

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strings"
	"unsafe"
)

// IdentityTypeC25519 is a classic Curve25519/Ed25519 identity
const IdentityTypeC25519 = 0

// IdentityTypeP384 is an identity containing both NIST P-384 and Curve25519/Ed25519 key types and leveraging both when possible
const IdentityTypeP384 = 1

// Sizes of components of different identity types
const (
	IdentityTypeC25519PublicKeySize  = 64  // C25519/Ed25519 keys
	IdentityTypeC25519PrivateKeySize = 64  // C25519/Ed25519 private keys
	IdentityTypeP384PublicKeySize    = 209 // C25519/Ed25519, P-384 point-compressed public, P-384 self-signature
	IdentityTypeP384PrivateKeySize   = 112 // C25519/Ed25519 and P-384 private keys
)

// Identity is precisely what it sounds like: the address and associated keys for a ZeroTier node
type Identity struct {
	address    Address
	idtype     int
	publicKey  []byte
	privateKey []byte
}

func newIdentityFromCIdentity(cid unsafe.Pointer) (*Identity, error) {
	if uintptr(cid) == 0 {
		return nil, ErrInvalidParameter
	}
	var idStrBuf [4096]byte
	idStr := C.ZT_Identity_toString(cid,(*C.char)(unsafe.Pointer(&idStrBuf[0])),4096,1)
	if uintptr(unsafe.Pointer(idStr)) == 0 {
		return nil, ErrInternal
	}
	return NewIdentityFromString(C.GoString(idStr))
}

// NewIdentity generates a new identity of the selected type
func NewIdentity(identityType int) (*Identity, error) {
	cid := C.ZT_Identity_new(C.enum_ZT_Identity_Type(identityType))
	if uintptr(unsafe.Pointer(cid)) == 0 {
		return nil, ErrInternal
	}
	defer C.ZT_Identity_delete(cid)
	return newIdentityFromCIdentity(cid)
}

// NewIdentityFromString generates a new identity from its string representation.
// The private key is imported as well if it is present.
func NewIdentityFromString(s string) (*Identity, error) {
	ss := strings.Split(strings.TrimSpace(s), ":")
	if len(ss) < 3 {
		return nil, ErrInvalidParameter
	}

	var err error
	var id Identity
	id.address, err = NewAddressFromString(ss[0])
	if err != nil {
		return nil, err
	}

	if ss[1] == "0" {
		id.idtype = 0
	} else if ss[1] == "1" {
		id.idtype = 1
	} else {
		return nil, ErrUnrecognizedIdentityType
	}

	switch id.idtype {

	case 0:
		id.publicKey, err = hex.DecodeString(ss[2])
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
		id.publicKey, err = Base32StdLowerCase.DecodeString(ss[2])
		if err != nil {
			return nil, err
		}
		if len(id.publicKey) != IdentityTypeP384PublicKeySize {
			return nil, ErrInvalidKey
		}
		if len(ss) >= 4 {
			id.privateKey, err = Base32StdLowerCase.DecodeString(ss[3])
			if err != nil {
				return nil, err
			}
			if len(id.privateKey) != IdentityTypeP384PrivateKeySize {
				return nil, ErrInvalidKey
			}
		}

	}

	return &id, nil
}

// Address returns this identity's address
func (id *Identity) Address() Address { return id.address }

// HasPrivate returns true if this identity has its own private portion.
func (id *Identity) HasPrivate() bool { return len(id.privateKey) > 0 }

// PrivateKeyString returns the full identity.secret if the private key is set, or an empty string if no private key is set.
func (id *Identity) PrivateKeyString() string {
	switch id.idtype {
	case IdentityTypeC25519:
		if len(id.publicKey) == IdentityTypeC25519PublicKeySize && len(id.privateKey) == IdentityTypeC25519PrivateKeySize {
			return fmt.Sprintf("%.10x:0:%x:%x", uint64(id.address), id.publicKey, id.privateKey)
		}
	case IdentityTypeP384:
		if len(id.publicKey) == IdentityTypeP384PublicKeySize && len(id.privateKey) == IdentityTypeP384PrivateKeySize {
			return fmt.Sprintf("%.10x:1:%s:%s", uint64(id.address), Base32StdLowerCase.EncodeToString(id.publicKey), Base32StdLowerCase.EncodeToString(id.privateKey))
		}
	}
	return ""
}

// PublicKeyString returns the address and public key (identity.public contents).
// An empty string is returned if this identity is invalid or not initialized.
func (id *Identity) String() string {
	switch id.idtype {
	case IdentityTypeC25519:
		if len(id.publicKey) == IdentityTypeC25519PublicKeySize {
			return fmt.Sprintf("%.10x:0:%x", uint64(id.address), id.publicKey)
		}
	case IdentityTypeP384:
		if len(id.publicKey) == IdentityTypeP384PublicKeySize {
			return fmt.Sprintf("%.10x:1:%s", uint64(id.address), Base32StdLowerCase.EncodeToString(id.publicKey))
		}
	}
	return ""
}

// LocallyValidate performs local self-validation of this identity
func (id *Identity) LocallyValidate() bool {
	idCStr := C.CString(id.String())
	defer C.free(unsafe.Pointer(idCStr))
	cid := C.ZT_Identity_fromString(idCStr)
	if uintptr(cid) == 0 {
		return false
	}
	defer C.ZT_Identity_delete(cid)
	return C.ZT_Identity_validate(cid) != 0
}

// Sign signs a message with this identity
func (id *Identity) Sign(msg []byte) ([]byte, error) {
	idCStr := C.CString(id.PrivateKeyString())
	defer C.free(unsafe.Pointer(idCStr))
	cid := C.ZT_Identity_fromString(idCStr)
	if uintptr(cid) == 0 {
		return nil, ErrInvalidKey
	}
	defer C.ZT_Identity_delete(cid)

	var dataP unsafe.Pointer
	if len(msg) > 0 {
		dataP = unsafe.Pointer(&msg[0])
	}
	var sig [96]byte
	sigLen := C.ZT_Identity_sign(cid,dataP,C.uint(len(msg)),unsafe.Pointer(&sig[0]),96)
	if sigLen <= 0 {
		return nil, ErrInvalidKey
	}

	return sig[0:uint(sigLen)], nil
}

// Verify verifies a signature
func (id *Identity) Verify(msg, sig []byte) bool {
	if len(sig) == 0 {
		return false
	}

	idCStr := C.CString(id.String())
	defer C.free(unsafe.Pointer(idCStr))
	cid := C.ZT_Identity_fromString(idCStr)
	if uintptr(cid) == 0 {
		return false
	}
	defer C.ZT_Identity_delete(cid)

	var dataP unsafe.Pointer
	if len(msg) > 0 {
		dataP = unsafe.Pointer(&msg[0])
	}
	return C.ZT_Identity_verify(cid,dataP,C.uint(len(msg)),unsafe.Pointer(&sig[0]),C.uint(len(sig))) != 0
}

// MarshalJSON marshals this Identity in its string format (private key is never included)
func (id *Identity) MarshalJSON() ([]byte, error) {
	return []byte("\"" + id.String() + "\""), nil
}

// UnmarshalJSON unmarshals this Identity from a string
func (id *Identity) UnmarshalJSON(j []byte) error {
	var s string
	err := json.Unmarshal(j, &s)
	if err != nil {
		return err
	}
	nid, err := NewIdentityFromString(s)
	if err != nil {
		return err
	}
	*id = *nid
	return nil
}
