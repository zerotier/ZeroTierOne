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

package ztnode

/*

// errTypeName returns the type name of an error minus any leading * character.
func errTypeName(err error) string {
	if err == nil {
		return ""
	}
	et := reflect.TypeOf(err)
	if et.Kind() == reflect.Ptr {
		return et.Elem().Name()
	}
	return et.Name()
}

//////////////////////////////////////////////////////////////////////////////

// Err indicates a general LF error such as an invalid parameter or state.
type Err string

func (e Err) Error() string { return (string)(e) }

// General errors
const (
	ErrInvalidPublicKey       Err = "invalid public key"
	ErrInvalidPrivateKey      Err = "invalid private key"
	ErrInvalidParameter       Err = "invalid parameter"
	ErrInvalidObject          Err = "invalid object"
	ErrUnsupportedType        Err = "unsupported type"
	ErrUnsupportedCurve       Err = "unsupported ECC curve (for this purpose)"
	ErrOutOfRange             Err = "parameter out of range"
	ErrWharrgarblFailed       Err = "Wharrgarbl proof of work algorithm failed (out of memory?)"
	ErrIO                     Err = "I/O error"
	ErrIncorrectKey           Err = "incorrect key"
	ErrAlreadyConnected       Err = "already connected"
	ErrRecordNotFound         Err = "record not found"
	ErrRecordIsNewer          Err = "record is newer than timestamp"
	ErrPulseSpanExeceeded     Err = "pulse is more than one year after record"
	ErrDuplicateRecord        Err = "duplicate record"
	ErrPrivateKeyRequired     Err = "private key required"
	ErrInvalidMessageSize     Err = "message size invalid"
	ErrQueryRequiresSelectors Err = "query requires at least one selector"
	ErrQueryInvalidSortOrder  Err = "invalid sort order value"
	ErrAlreadyMounted         Err = "mount point already mounted"
)

//////////////////////////////////////////////////////////////////////////////

// ErrRecord indicates an error related to an invalid record or a record failing a check.
type ErrRecord string

func (e ErrRecord) Error() string { return (string)(e) }

// Errs indicating that a record is invalid
const (
	ErrRecordInvalid                   ErrRecord = "record invalid"
	ErrRecordOwnerSignatureCheckFailed ErrRecord = "owner signature check failed"
	ErrRecordInsufficientWork          ErrRecord = "insufficient work to pay for this record"
	ErrRecordNotApproved               ErrRecord = "record not currently approved (via proof of work and/or certificates)"
	ErrRecordInsufficientLinks         ErrRecord = "insufficient links"
	ErrRecordTooManyLinks              ErrRecord = "too many links"
	ErrRecordInvalidLinks              ErrRecord = "links must be sorted and unique"
	ErrRecordTooManySelectors          ErrRecord = "too many selectors"
	ErrRecordUnsupportedAlgorithm      ErrRecord = "unsupported algorithm or type"
	ErrRecordTooLarge                  ErrRecord = "record too large"
	ErrRecordValueTooLarge             ErrRecord = "record value too large"
	ErrRecordViolatesSpecialRelativity ErrRecord = "record timestamp too far in the future"
	ErrRecordTooOld                    ErrRecord = "record older than network timestamp floor"
	ErrRecordCertificateInvalid        ErrRecord = "certificate invalid"
	ErrRecordCertificateRequired       ErrRecord = "certificate required"
	ErrRecordProhibited                ErrRecord = "record administratively prohibited"
)

//////////////////////////////////////////////////////////////////////////////

// ErrDatabase contains information about a database related problem.
type ErrDatabase struct {
	// ErrCode is the error code returned by the C database module.
	ErrCode int

	// ErrMessage is an error message supplied by the C code or by Go (optional)
	ErrMessage string
}

func (e ErrDatabase) Error() string {
	return fmt.Sprintf("database error: %d (%s)", e.ErrCode, e.ErrMessage)
}

//////////////////////////////////////////////////////////////////////////////

// ErrAPI (response) indicates an error and is returned with non-200 responses.
type ErrAPI struct {
	Code        int    ``                  // HTTP response code
	Message     string `json:",omitempty"` // Message indicating the reason for the error
	ErrTypeName string `json:",omitempty"` // Name of LF native error or empty if HTTP or transport error
}

// Error implements the error interface, making APIError an 'error' in the Go sense.
func (e ErrAPI) Error() string {
	if len(e.ErrTypeName) > 0 {
		return fmt.Sprintf("%d:%s:%s", e.Code, e.ErrTypeName, e.Message)
	}
	return fmt.Sprintf("%d:%s", e.Code, e.Message)
}

//////////////////////////////////////////////////////////////////////////////

*/
