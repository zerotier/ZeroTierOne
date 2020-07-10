/*
 * Copyright (C)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package zerotier

// #include "../../serviceiocore/GoGlue.h"
import "C"

import (
	"bytes"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"runtime"
	"strings"
	"unsafe"
)

// Constants from node/Identity.hpp (must be the same)
const (
	IdentityTypeC25519 = 0
	IdentityTypeP384   = 1

	IdentityTypeC25519PublicKeySize  = 64
	IdentityTypeC25519PrivateKeySize = 64
	IdentityTypeP384PublicKeySize    = 114
	IdentityTypeP384PrivateKeySize   = 112
)

// Identity is precisely what it sounds like: the address and associated keys for a ZeroTier node
type Identity struct {
	address    Address
	idtype     int
	publicKey  []byte
	privateKey []byte
	cid        unsafe.Pointer
}

func identityFinalizer(obj interface{}) {
	cid := obj.(*Identity).cid
	if cid != nil {
		C.ZT_Identity_delete(cid)
	}
}

func newIdentityFromCIdentity(cid unsafe.Pointer) (*Identity, error) {
	if cid == nil {
		return nil, ErrInvalidParameter
	}

	var idStrBuf [4096]byte
	idStr := C.ZT_Identity_toString(cid, (*C.char)(unsafe.Pointer(&idStrBuf[0])), 4096, 1)
	if uintptr(unsafe.Pointer(idStr)) == 0 {
		return nil, ErrInternal
	}

	id, err := NewIdentityFromString(C.GoString(idStr))
	if err != nil {
		return nil, err
	}

	runtime.SetFinalizer(id, identityFinalizer)

	return id, nil
}

// initCIdentityPtr returns a pointer to the core ZT_Identity instance or nil/0 on error.
func (id *Identity) cIdentity() unsafe.Pointer {
	if id.cid == nil {
		str := []byte(id.PrivateKeyString())
		if len(str) == 0 {
			str = []byte(id.String())
		}
		if len(str) == 0 {
			return nil
		}
		str = append(str, byte(0))
		id.cid = C.ZT_Identity_fromString((*C.char)(unsafe.Pointer(&str[0])))
	}
	return id.cid
}

// NewIdentity generates a new identity of the selected type.
func NewIdentity(identityType int) (*Identity, error) {
	var cid unsafe.Pointer
	switch identityType {
	case C.ZT_IDENTITY_TYPE_C25519:
		cid = C.ZT_Identity_new(C.ZT_IDENTITY_TYPE_C25519)
	case C.ZT_IDENTITY_TYPE_P384:
		cid = C.ZT_Identity_new(C.ZT_IDENTITY_TYPE_P384)
	default:
		return nil, ErrInvalidParameter
	}
	id, err := newIdentityFromCIdentity(cid)
	if err != nil {
		return nil, err
	}
	id.cid = cid
	return id, nil
}

// NewIdentityFromString generates a new identity from its string representation.
// The private key is imported as well if it is present.
func NewIdentityFromString(s string) (*Identity, error) {
	ss := strings.Split(strings.TrimSpace(s), ":")
	if len(ss) < 3 {
		return nil, ErrInvalidParameter
	}

	var err error
	id := new(Identity)
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

	return id, nil
}

// Address returns this identity's address.
func (id *Identity) Address() Address { return id.address }

// HasPrivate returns true if this identity has its own private portion.
func (id *Identity) HasPrivate() bool { return len(id.privateKey) > 0 }

// Fingerprint gets this identity's address plus hash of public key(s).
func (id *Identity) Fingerprint() *Fingerprint {
	return newFingerprintFromCFingerprint(C.ZT_Identity_fingerprint(id.cIdentity()))
}

// PrivateKeyString returns the full identity.secret if the private key is set,
// or an empty string if no private key is set.
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
	return C.ZT_Identity_validate(id.cIdentity()) != 0
}

// Sign signs a message with this identity
func (id *Identity) Sign(msg []byte) ([]byte, error) {
	var dataP unsafe.Pointer
	if len(msg) > 0 {
		dataP = unsafe.Pointer(&msg[0])
	}
	var sig [96]byte
	sigLen := C.ZT_Identity_sign(id.cIdentity(), dataP, C.uint(len(msg)), unsafe.Pointer(&sig[0]), 96)
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
	var dataP unsafe.Pointer
	if len(msg) > 0 {
		dataP = unsafe.Pointer(&msg[0])
	}
	return C.ZT_Identity_verify(id.cIdentity(), dataP, C.uint(len(msg)), unsafe.Pointer(&sig[0]), C.uint(len(sig))) != 0
}

// Equals performs a deep equality test between this and another identity
func (id *Identity) Equals(id2 *Identity) bool {
	if id2 == nil {
		return id == nil
	}
	if id == nil {
		return false
	}
	return id.address == id2.address && id.idtype == id2.idtype && bytes.Equal(id.publicKey, id2.publicKey) && bytes.Equal(id.privateKey, id2.privateKey)
}

func (id *Identity) MarshalJSON() ([]byte, error) {
	return []byte("\"" + id.String() + "\""), nil
}

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
