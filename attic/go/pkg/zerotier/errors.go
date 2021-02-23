/*
 * Copyright (C)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package zerotier

// Err is a basic string error type for ZeroTier
type Err string

func (e Err) Error() string { return (string)(e) }

// Simple ZeroTier Errors
const (
	ErrInternal                 Err = "internal error"
	ErrNodeInitFailed           Err = "unable to initialize core Node instance"
	ErrInvalidMACAddress        Err = "invalid MAC address"
	ErrInvalidZeroTierAddress   Err = "invalid ZeroTier address"
	ErrInvalidNetworkID         Err = "invalid network ID"
	ErrInvalidParameter         Err = "invalid parameter"
	ErrTapInitFailed            Err = "unable to create native Tap instance"
	ErrUnrecognizedIdentityType Err = "unrecognized identity type"
	ErrInvalidKey               Err = "invalid key data"

	ErrCertificateHaveNewerCert             Err = "a newer certificate for this subject unique ID is already loaded"
	ErrCertificateInvalidFormat             Err = "invalid certificate format"
	ErrCertificateInvalidIdentity           Err = "invalid identity in certificate"
	ErrCertificateInvalidPrimarySignature   Err = "invalid primary signature"
	ErrCertificateInvalidChain              Err = "certificate chain verification failed"
	ErrCertificateInvalidComponentSignature Err = "an internal component of this certificate has an invalid signature"
	ErrCertificateInvalidUniqueIDProof      Err = "certificate subject unique ID proof signature verification failed"
	ErrCertificateMissingRequiredFields     Err = "certificate is missing one or more required fields"
	ErrCertificateOutOfValidTimeWindow      Err = "certificate is out of its valid time window"
)

// APIErr is returned by the JSON API when a call fails
type APIErr struct {
	Reason string
}

func (e *APIErr) Error() string { return e.Reason }
