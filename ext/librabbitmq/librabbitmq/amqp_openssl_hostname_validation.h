#ifndef librabbitmq_amqp_openssl_hostname_validation_h
#define librabbitmq_amqp_openssl_hostname_validation_h

/*
 * Copyright (C) 2012, iSEC Partners.
 * Copyright (C) 2015 Alan Antonuk.
 *
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization of the
 * copyright holder.
 */

/* Originally from:
 * https://github.com/iSECPartners/ssl-conservatory
 * https://wiki.openssl.org/index.php/Hostname_validation
 */

#include <openssl/x509v3.h>

typedef enum {
  AMQP_HVR_MATCH_FOUND,
  AMQP_HVR_MATCH_NOT_FOUND,
  AMQP_HVR_NO_SAN_PRESENT,
  AMQP_HVR_MALFORMED_CERTIFICATE,
  AMQP_HVR_ERROR
} amqp_hostname_validation_result;

/**
* Validates the server's identity by looking for the expected hostname in the
* server's certificate. As described in RFC 6125, it first tries to find a match
* in the Subject Alternative Name extension. If the extension is not present in
* the certificate, it checks the Common Name instead.
*
* Returns AMQP_HVR_MATCH_FOUND if a match was found.
* Returns AMQP_HVR_MATCH_NOT_FOUND if no matches were found.
* Returns AMQP_HVR_MALFORMED_CERTIFICATE if any of the hostnames had a NUL
* character embedded in it.
* Returns AMQP_HVR_ERROR if there was an error.
*/
amqp_hostname_validation_result amqp_ssl_validate_hostname(
    const char *hostname, const X509 *server_cert);

#endif
