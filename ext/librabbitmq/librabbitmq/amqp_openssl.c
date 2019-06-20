/*
 * Portions created by Alan Antonuk are Copyright (c) 2012-2014 Alan Antonuk.
 * All Rights Reserved.
 *
 * Portions created by Michael Steinert are Copyright (c) 2012-2014 Michael
 * Steinert. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "amqp_openssl_bio.h"
#include "amqp_openssl_hostname_validation.h"
#include "amqp_private.h"
#include "amqp_socket.h"
#include "amqp_ssl_socket.h"
#include "amqp_time.h"
#include "threads.h"

#include <ctype.h>
#include <limits.h>
#include <openssl/bio.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <stdlib.h>
#include <string.h>

static int initialize_ssl_and_increment_connections(void);
static int decrement_ssl_connections(void);

static unsigned long ssl_threadid_callback(void);
static void ssl_locking_callback(int mode, int n, const char *file, int line);
static pthread_mutex_t *amqp_openssl_lockarray = NULL;

static pthread_mutex_t openssl_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static amqp_boolean_t do_initialize_openssl = 1;
static amqp_boolean_t openssl_initialized = 0;
static amqp_boolean_t openssl_bio_initialized = 0;
static int openssl_connections = 0;

#define CHECK_SUCCESS(condition)                                            \
  do {                                                                      \
    int check_success_ret = (condition);                                    \
    if (check_success_ret) {                                                \
      amqp_abort("Check %s failed <%d>: %s", #condition, check_success_ret, \
                 strerror(check_success_ret));                              \
    }                                                                       \
  } while (0)

struct amqp_ssl_socket_t {
  const struct amqp_socket_class_t *klass;
  SSL_CTX *ctx;
  int sockfd;
  SSL *ssl;
  amqp_boolean_t verify_peer;
  amqp_boolean_t verify_hostname;
  int internal_error;
};

static ssize_t amqp_ssl_socket_send(void *base, const void *buf, size_t len,
                                    AMQP_UNUSED int flags) {
  struct amqp_ssl_socket_t *self = (struct amqp_ssl_socket_t *)base;
  int res;
  if (-1 == self->sockfd) {
    return AMQP_STATUS_SOCKET_CLOSED;
  }

  /* SSL_write takes an int for length of buffer, protect against len being
   * larger than larger than what SSL_write can take */
  if (len > INT_MAX) {
    return AMQP_STATUS_INVALID_PARAMETER;
  }

  ERR_clear_error();
  self->internal_error = 0;

  /* This will only return on error, or once the whole buffer has been
   * written to the SSL stream. See SSL_MODE_ENABLE_PARTIAL_WRITE */
  res = SSL_write(self->ssl, buf, (int)len);
  if (0 >= res) {
    self->internal_error = SSL_get_error(self->ssl, res);
    /* TODO: Close connection if it isn't already? */
    /* TODO: Possibly be more intelligent in reporting WHAT went wrong */
    switch (self->internal_error) {
      case SSL_ERROR_WANT_READ:
        res = AMQP_PRIVATE_STATUS_SOCKET_NEEDREAD;
        break;
      case SSL_ERROR_WANT_WRITE:
        res = AMQP_PRIVATE_STATUS_SOCKET_NEEDWRITE;
        break;
      case SSL_ERROR_ZERO_RETURN:
        res = AMQP_STATUS_CONNECTION_CLOSED;
        break;
      default:
        res = AMQP_STATUS_SSL_ERROR;
        break;
    }
  } else {
    self->internal_error = 0;
  }

  return (ssize_t)res;
}

static ssize_t amqp_ssl_socket_recv(void *base, void *buf, size_t len,
                                    AMQP_UNUSED int flags) {
  struct amqp_ssl_socket_t *self = (struct amqp_ssl_socket_t *)base;
  int received;
  if (-1 == self->sockfd) {
    return AMQP_STATUS_SOCKET_CLOSED;
  }

  /* SSL_read takes an int for length of buffer, protect against len being
   * larger than larger than what SSL_read can take */
  if (len > INT_MAX) {
    return AMQP_STATUS_INVALID_PARAMETER;
  }

  ERR_clear_error();
  self->internal_error = 0;

  received = SSL_read(self->ssl, buf, (int)len);
  if (0 >= received) {
    self->internal_error = SSL_get_error(self->ssl, received);
    switch (self->internal_error) {
      case SSL_ERROR_WANT_READ:
        received = AMQP_PRIVATE_STATUS_SOCKET_NEEDREAD;
        break;
      case SSL_ERROR_WANT_WRITE:
        received = AMQP_PRIVATE_STATUS_SOCKET_NEEDWRITE;
        break;
      case SSL_ERROR_ZERO_RETURN:
        received = AMQP_STATUS_CONNECTION_CLOSED;
        break;
      default:
        received = AMQP_STATUS_SSL_ERROR;
        break;
    }
  }

  return (ssize_t)received;
}

static int amqp_ssl_socket_open(void *base, const char *host, int port,
                                struct timeval *timeout) {
  struct amqp_ssl_socket_t *self = (struct amqp_ssl_socket_t *)base;
  long result;
  int status;
  amqp_time_t deadline;
  X509 *cert;
  BIO *bio;
  if (-1 != self->sockfd) {
    return AMQP_STATUS_SOCKET_INUSE;
  }
  ERR_clear_error();

  self->ssl = SSL_new(self->ctx);
  if (!self->ssl) {
    self->internal_error = ERR_peek_error();
    status = AMQP_STATUS_SSL_ERROR;
    goto exit;
  }

  status = amqp_time_from_now(&deadline, timeout);
  if (AMQP_STATUS_OK != status) {
    return status;
  }

  self->sockfd = amqp_open_socket_inner(host, port, deadline);
  if (0 > self->sockfd) {
    status = self->sockfd;
    self->internal_error = amqp_os_socket_error();
    self->sockfd = -1;
    goto error_out1;
  }

  bio = BIO_new(amqp_openssl_bio());
  if (!bio) {
    status = AMQP_STATUS_NO_MEMORY;
    goto error_out2;
  }

  BIO_set_fd(bio, self->sockfd, BIO_NOCLOSE);
  SSL_set_bio(self->ssl, bio, bio);

  status = SSL_set_tlsext_host_name(self->ssl, host);
  if (!status) {
    self->internal_error = SSL_get_error(self->ssl, status);
    status = AMQP_STATUS_SSL_ERROR;
    goto error_out2;
  }

start_connect:
  status = SSL_connect(self->ssl);
  if (status != 1) {
    self->internal_error = SSL_get_error(self->ssl, status);
    switch (self->internal_error) {
      case SSL_ERROR_WANT_READ:
        status = amqp_poll(self->sockfd, AMQP_SF_POLLIN, deadline);
        break;
      case SSL_ERROR_WANT_WRITE:
        status = amqp_poll(self->sockfd, AMQP_SF_POLLOUT, deadline);
        break;
      default:
        status = AMQP_STATUS_SSL_CONNECTION_FAILED;
    }
    if (AMQP_STATUS_OK == status) {
      goto start_connect;
    }
    goto error_out2;
  }

  cert = SSL_get_peer_certificate(self->ssl);

  if (self->verify_peer) {
    if (!cert) {
      self->internal_error = 0;
      status = AMQP_STATUS_SSL_PEER_VERIFY_FAILED;
      goto error_out3;
    }

    result = SSL_get_verify_result(self->ssl);
    if (X509_V_OK != result) {
      self->internal_error = result;
      status = AMQP_STATUS_SSL_PEER_VERIFY_FAILED;
      goto error_out4;
    }
  }
  if (self->verify_hostname) {
    if (!cert) {
      self->internal_error = 0;
      status = AMQP_STATUS_SSL_HOSTNAME_VERIFY_FAILED;
      goto error_out3;
    }

    if (AMQP_HVR_MATCH_FOUND != amqp_ssl_validate_hostname(host, cert)) {
      self->internal_error = 0;
      status = AMQP_STATUS_SSL_HOSTNAME_VERIFY_FAILED;
      goto error_out4;
    }
  }

  X509_free(cert);
  self->internal_error = 0;
  status = AMQP_STATUS_OK;

exit:
  return status;

error_out4:
  X509_free(cert);
error_out3:
  SSL_shutdown(self->ssl);
error_out2:
  amqp_os_socket_close(self->sockfd);
  self->sockfd = -1;
error_out1:
  SSL_free(self->ssl);
  self->ssl = NULL;
  goto exit;
}

static int amqp_ssl_socket_close(void *base, amqp_socket_close_enum force) {
  struct amqp_ssl_socket_t *self = (struct amqp_ssl_socket_t *)base;

  if (-1 == self->sockfd) {
    return AMQP_STATUS_SOCKET_CLOSED;
  }

  if (AMQP_SC_NONE == force) {
    /* don't try too hard to shutdown the connection */
    SSL_shutdown(self->ssl);
  }

  SSL_free(self->ssl);
  self->ssl = NULL;

  if (amqp_os_socket_close(self->sockfd)) {
    return AMQP_STATUS_SOCKET_ERROR;
  }
  self->sockfd = -1;

  return AMQP_STATUS_OK;
}

static int amqp_ssl_socket_get_sockfd(void *base) {
  struct amqp_ssl_socket_t *self = (struct amqp_ssl_socket_t *)base;
  return self->sockfd;
}

static void amqp_ssl_socket_delete(void *base) {
  struct amqp_ssl_socket_t *self = (struct amqp_ssl_socket_t *)base;

  if (self) {
    amqp_ssl_socket_close(self, AMQP_SC_NONE);

    SSL_CTX_free(self->ctx);
    free(self);
  }
  decrement_ssl_connections();
}

static const struct amqp_socket_class_t amqp_ssl_socket_class = {
    amqp_ssl_socket_send,       /* send */
    amqp_ssl_socket_recv,       /* recv */
    amqp_ssl_socket_open,       /* open */
    amqp_ssl_socket_close,      /* close */
    amqp_ssl_socket_get_sockfd, /* get_sockfd */
    amqp_ssl_socket_delete      /* delete */
};

amqp_socket_t *amqp_ssl_socket_new(amqp_connection_state_t state) {
  struct amqp_ssl_socket_t *self = calloc(1, sizeof(*self));
  int status;
  if (!self) {
    return NULL;
  }

  self->sockfd = -1;
  self->klass = &amqp_ssl_socket_class;
  self->verify_peer = 1;
  self->verify_hostname = 1;

  status = initialize_ssl_and_increment_connections();
  if (status) {
    goto error;
  }

  self->ctx = SSL_CTX_new(SSLv23_client_method());
  if (!self->ctx) {
    goto error;
  }
  /* Disable SSLv2 and SSLv3 */
  SSL_CTX_set_options(self->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

  amqp_set_socket(state, (amqp_socket_t *)self);

  return (amqp_socket_t *)self;
error:
  amqp_ssl_socket_delete((amqp_socket_t *)self);
  return NULL;
}

int amqp_ssl_socket_set_cacert(amqp_socket_t *base, const char *cacert) {
  int status;
  struct amqp_ssl_socket_t *self;
  if (base->klass != &amqp_ssl_socket_class) {
    amqp_abort("<%p> is not of type amqp_ssl_socket_t", base);
  }
  self = (struct amqp_ssl_socket_t *)base;
  status = SSL_CTX_load_verify_locations(self->ctx, cacert, NULL);
  if (1 != status) {
    return AMQP_STATUS_SSL_ERROR;
  }
  return AMQP_STATUS_OK;
}

int amqp_ssl_socket_set_key(amqp_socket_t *base, const char *cert,
                            const char *key) {
  int status;
  struct amqp_ssl_socket_t *self;
  if (base->klass != &amqp_ssl_socket_class) {
    amqp_abort("<%p> is not of type amqp_ssl_socket_t", base);
  }
  self = (struct amqp_ssl_socket_t *)base;
  status = SSL_CTX_use_certificate_chain_file(self->ctx, cert);
  if (1 != status) {
    return AMQP_STATUS_SSL_ERROR;
  }
  status = SSL_CTX_use_PrivateKey_file(self->ctx, key, SSL_FILETYPE_PEM);
  if (1 != status) {
    return AMQP_STATUS_SSL_ERROR;
  }
  return AMQP_STATUS_OK;
}

static int password_cb(AMQP_UNUSED char *buffer, AMQP_UNUSED int length,
                       AMQP_UNUSED int rwflag, AMQP_UNUSED void *user_data) {
  amqp_abort("rabbitmq-c does not support password protected keys");
}

int amqp_ssl_socket_set_key_buffer(amqp_socket_t *base, const char *cert,
                                   const void *key, size_t n) {
  int status = AMQP_STATUS_OK;
  BIO *buf = NULL;
  RSA *rsa = NULL;
  struct amqp_ssl_socket_t *self;
  if (base->klass != &amqp_ssl_socket_class) {
    amqp_abort("<%p> is not of type amqp_ssl_socket_t", base);
  }
  if (n > INT_MAX) {
    return AMQP_STATUS_INVALID_PARAMETER;
  }
  self = (struct amqp_ssl_socket_t *)base;
  status = SSL_CTX_use_certificate_chain_file(self->ctx, cert);
  if (1 != status) {
    return AMQP_STATUS_SSL_ERROR;
  }
  buf = BIO_new_mem_buf((void *)key, (int)n);
  if (!buf) {
    goto error;
  }
  rsa = PEM_read_bio_RSAPrivateKey(buf, NULL, password_cb, NULL);
  if (!rsa) {
    goto error;
  }
  status = SSL_CTX_use_RSAPrivateKey(self->ctx, rsa);
  if (1 != status) {
    goto error;
  }
exit:
  BIO_vfree(buf);
  RSA_free(rsa);
  return status;
error:
  status = AMQP_STATUS_SSL_ERROR;
  goto exit;
}

int amqp_ssl_socket_set_cert(amqp_socket_t *base, const char *cert) {
  int status;
  struct amqp_ssl_socket_t *self;
  if (base->klass != &amqp_ssl_socket_class) {
    amqp_abort("<%p> is not of type amqp_ssl_socket_t", base);
  }
  self = (struct amqp_ssl_socket_t *)base;
  status = SSL_CTX_use_certificate_chain_file(self->ctx, cert);
  if (1 != status) {
    return AMQP_STATUS_SSL_ERROR;
  }
  return AMQP_STATUS_OK;
}

void amqp_ssl_socket_set_verify(amqp_socket_t *base, amqp_boolean_t verify) {
  amqp_ssl_socket_set_verify_peer(base, verify);
  amqp_ssl_socket_set_verify_hostname(base, verify);
}

void amqp_ssl_socket_set_verify_peer(amqp_socket_t *base,
                                     amqp_boolean_t verify) {
  struct amqp_ssl_socket_t *self;
  if (base->klass != &amqp_ssl_socket_class) {
    amqp_abort("<%p> is not of type amqp_ssl_socket_t", base);
  }
  self = (struct amqp_ssl_socket_t *)base;
  self->verify_peer = verify;
}

void amqp_ssl_socket_set_verify_hostname(amqp_socket_t *base,
                                         amqp_boolean_t verify) {
  struct amqp_ssl_socket_t *self;
  if (base->klass != &amqp_ssl_socket_class) {
    amqp_abort("<%p> is not of type amqp_ssl_socket_t", base);
  }
  self = (struct amqp_ssl_socket_t *)base;
  self->verify_hostname = verify;
}

int amqp_ssl_socket_set_ssl_versions(amqp_socket_t *base,
                                     amqp_tls_version_t min,
                                     amqp_tls_version_t max) {
  struct amqp_ssl_socket_t *self;
  if (base->klass != &amqp_ssl_socket_class) {
    amqp_abort("<%p> is not of type amqp_ssl_socket_t", base);
  }
  self = (struct amqp_ssl_socket_t *)base;

  {
    long clear_options;
    long set_options = 0;
#if defined(SSL_OP_NO_TLSv1_2)
    amqp_tls_version_t max_supported = AMQP_TLSv1_2;
    clear_options = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2;
#elif defined(SSL_OP_NO_TLSv1_1)
    amqp_tls_version_t max_supported = AMQP_TLSv1_1;
    clear_options = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1;
#elif defined(SSL_OP_NO_TLSv1)
    amqp_tls_version_t max_supported = AMQP_TLSv1;
    clear_options = SSL_OP_NO_TLSv1;
#else
#error "Need a version of OpenSSL that can support TLSv1 or greater."
#endif

    if (AMQP_TLSvLATEST == max) {
      max = max_supported;
    }
    if (AMQP_TLSvLATEST == min) {
      min = max_supported;
    }

    if (min > max) {
      return AMQP_STATUS_INVALID_PARAMETER;
    }

    if (max > max_supported || min > max_supported) {
      return AMQP_STATUS_UNSUPPORTED;
    }

    if (min > AMQP_TLSv1) {
      set_options |= SSL_OP_NO_TLSv1;
    }
#ifdef SSL_OP_NO_TLSv1_1
    if (min > AMQP_TLSv1_1 || max < AMQP_TLSv1_1) {
      set_options |= SSL_OP_NO_TLSv1_1;
    }
#endif
#ifdef SSL_OP_NO_TLSv1_2
    if (max < AMQP_TLSv1_2) {
      set_options |= SSL_OP_NO_TLSv1_2;
    }
#endif
    SSL_CTX_clear_options(self->ctx, clear_options);
    SSL_CTX_set_options(self->ctx, set_options);
  }

  return AMQP_STATUS_OK;
}

void amqp_set_initialize_ssl_library(amqp_boolean_t do_initialize) {
  CHECK_SUCCESS(pthread_mutex_lock(&openssl_init_mutex));

  if (openssl_connections == 0 && !openssl_initialized) {
    do_initialize_openssl = do_initialize;
  }
  CHECK_SUCCESS(pthread_mutex_unlock(&openssl_init_mutex));
}

static unsigned long ssl_threadid_callback(void) {
  return (unsigned long)pthread_self();
}

static void ssl_locking_callback(int mode, int n, AMQP_UNUSED const char *file,
                                 AMQP_UNUSED int line) {
  if (mode & CRYPTO_LOCK) {
    CHECK_SUCCESS(pthread_mutex_lock(&amqp_openssl_lockarray[n]));
  } else {
    CHECK_SUCCESS(pthread_mutex_unlock(&amqp_openssl_lockarray[n]));
  }
}

static int setup_openssl(void) {
  int status;

  int i;
  amqp_openssl_lockarray = calloc(CRYPTO_num_locks(), sizeof(pthread_mutex_t));
  if (!amqp_openssl_lockarray) {
    status = AMQP_STATUS_NO_MEMORY;
    goto out;
  }
  for (i = 0; i < CRYPTO_num_locks(); i++) {
    if (pthread_mutex_init(&amqp_openssl_lockarray[i], NULL)) {
      int j;
      for (j = 0; j < i; j++) {
        pthread_mutex_destroy(&amqp_openssl_lockarray[j]);
      }
      free(amqp_openssl_lockarray);
      status = AMQP_STATUS_SSL_ERROR;
      goto out;
    }
  }
  CRYPTO_set_id_callback(ssl_threadid_callback);
  CRYPTO_set_locking_callback(ssl_locking_callback);

#ifdef AMQP_OPENSSL_V110
  if (CONF_modules_load_file(NULL, "rabbitmq-c", CONF_MFLAGS_DEFAULT_SECTION) <=
      0) {
    status = AMQP_STATUS_SSL_ERROR;
    goto out;
  }
#else
  OPENSSL_config(NULL);
#endif
  SSL_library_init();
  SSL_load_error_strings();

  status = AMQP_STATUS_OK;
out:
  return status;
}

int amqp_initialize_ssl_library(void) {
  int status;
  CHECK_SUCCESS(pthread_mutex_lock(&openssl_init_mutex));

  if (!openssl_initialized) {
    status = setup_openssl();
    if (status) {
      goto out;
    }
    openssl_initialized = 1;
  }

  status = AMQP_STATUS_OK;
out:
  CHECK_SUCCESS(pthread_mutex_unlock(&openssl_init_mutex));
  return status;
}

static int initialize_ssl_and_increment_connections() {
  int status;
  CHECK_SUCCESS(pthread_mutex_lock(&openssl_init_mutex));

  if (do_initialize_openssl && !openssl_initialized) {
    status = setup_openssl();
    if (status) {
      goto exit;
    }
    openssl_initialized = 1;
  }

  if (!openssl_bio_initialized) {
    status = amqp_openssl_bio_init();
    if (status) {
      goto exit;
    }
    openssl_bio_initialized = 1;
  }

  openssl_connections += 1;
  status = AMQP_STATUS_OK;
exit:
  CHECK_SUCCESS(pthread_mutex_unlock(&openssl_init_mutex));
  return status;
}

static int decrement_ssl_connections(void) {
  CHECK_SUCCESS(pthread_mutex_lock(&openssl_init_mutex));

  if (openssl_connections > 0) {
    openssl_connections--;
  }

  CHECK_SUCCESS(pthread_mutex_unlock(&openssl_init_mutex));
  return AMQP_STATUS_OK;
}

int amqp_uninitialize_ssl_library(void) {
  int status;
  CHECK_SUCCESS(pthread_mutex_lock(&openssl_init_mutex));

  if (openssl_connections > 0) {
    status = AMQP_STATUS_SOCKET_INUSE;
    goto out;
  }

  amqp_openssl_bio_destroy();
  openssl_bio_initialized = 0;

#ifndef AMQP_OPENSSL_V110
  ERR_remove_state(0);
#endif

#ifndef LIBRESSL_VERSION_NUMBER
  FIPS_mode_set(0);
#endif

  CRYPTO_set_locking_callback(NULL);
  CRYPTO_set_id_callback(NULL);
  {
    int i;
    for (i = 0; i < CRYPTO_num_locks(); i++) {
      pthread_mutex_destroy(&amqp_openssl_lockarray[i]);
    }
    free(amqp_openssl_lockarray);
  }

  ENGINE_cleanup();
  CONF_modules_free();
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  ERR_free_strings();
#if (OPENSSL_VERSION_NUMBER >= 0x10002003L) && !defined(LIBRESSL_VERSION_NUMBER)
  SSL_COMP_free_compression_methods();
#endif

  openssl_initialized = 0;

  status = AMQP_STATUS_OK;
out:
  CHECK_SUCCESS(pthread_mutex_unlock(&openssl_init_mutex));
  return status;
}
