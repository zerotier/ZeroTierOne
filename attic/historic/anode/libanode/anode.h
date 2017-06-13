/* libanode: the Anode C reference implementation
 * Copyright (C) 2009-2010 Adam Ierymenko <adam.ierymenko@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _ANODE_ANODE_H
#define _ANODE_ANODE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define ANODE_ADDRESS_LENGTH_ANODE_256_40 40
#define ANODE_ADDRESS_MAX_LENGTH 40
#define ANODE_ADDRESS_SECRET_LENGTH_ANODE_256_40 32
#define ANODE_ADDRESS_MAX_SECRET_LENGTH 32

#define ANODE_ADDRESS_ID_LENGTH 8
#define ANODE_ZONE_LENGTH 4

#define ANODE_ERR_NONE 0
#define ANODE_ERR_INVALID_ARGUMENT (-10000)
#define ANODE_ERR_OUT_OF_MEMORY (-10001)
#define ANODE_ERR_INVALID_URI (-10002)
#define ANODE_ERR_BUFFER_TOO_SMALL (-10003)
#define ANODE_ERR_ADDRESS_INVALID (-10010)
#define ANODE_ERR_ADDRESS_TYPE_NOT_SUPPORTED (-10011)
#define ANODE_ERR_CONNECTION_CLOSED (-10012)
#define ANODE_ERR_CONNECTION_CLOSED_BY_REMOTE (-10013)
#define ANODE_ERR_CONNECT_FAILED (-10014)
#define ANODE_ERR_UNABLE_TO_BIND (-10015)
#define ANODE_ERR_TOO_MANY_OPEN_SOCKETS (-10016)
#define ANODE_ERR_DNS_NAME_NOT_FOUND_OR_TIMED_OUT (-10017)

/**
 * Get a human-readable error description for an error code
 *
 * The value of 'err' can be either negative or positive.
 *
 * @param err Error code
 * @return Human-readable description
 */
extern const char *Anode_strerror(int err);

/* ----------------------------------------------------------------------- */
/* Secure random source                                                    */
/* ----------------------------------------------------------------------- */

/**
 * Opaque secure random instance
 */
typedef void AnodeSecureRandom;

/**
 * Initialize a secure random source
 *
 * No cleanup/destructor is necessary.
 *
 * @param srng Random structure to initialize
 */
extern AnodeSecureRandom *AnodeSecureRandom_new();

/**
 * Generate random bytes
 *
 * @param srng Secure random source
 * @param buf Buffer to fill
 * @param count Number of bytes to generate
 */
extern void AnodeSecureRandom_gen_bytes(AnodeSecureRandom *srng,void *buf,long count);

/**
 * Destroy and free a secure random instance
 *
 * @param srng Secure random source
 */
extern void AnodeSecureRandom_delete(AnodeSecureRandom *srng);

/* ----------------------------------------------------------------------- */
/* AES-256 derived Davis-Meyer hash function                               */
/* ----------------------------------------------------------------------- */

/**
 * Digest a message using AES-DIGEST to yield a 16-byte hash code
 *
 * @param message Message to digest
 * @param message_len Length of message in bytes
 * @param hash Buffer to store 16 byte hash code
 */
extern void Anode_aes_digest(
  const void *const message,
  unsigned long message_len,
  void *const hash);

/* ----------------------------------------------------------------------- */
/* Address Types and Components                                            */
/* ----------------------------------------------------------------------- */

/**
 * Anode address
 *
 * The first byte always identifies the address type, which right now can
 * only be type 1 (ANODE-256-40).
 */
typedef struct
{
  char bits[ANODE_ADDRESS_MAX_LENGTH];
} AnodeAddress;

/**
 * 8-byte short Anode address ID
 */
typedef struct
{
  char bits[ANODE_ADDRESS_ID_LENGTH];
} AnodeAddressId;

/**
 * 4-byte Anode zone ID
 */
typedef struct
{
  char bits[ANODE_ZONE_LENGTH];
} AnodeZone;

/**
 * Anode address types
 */
enum AnodeAddressType
{
  ANODE_ADDRESS_ANODE_256_40 = 1
};

/**
 * Get the type of an Anode address
 *
 * This is a shortcut macro for just looking at the first byte and casting
 * it to the AnodeAddressType enum.
 *
 * @param a Pointer to address
 * @return Type as enum AnodeAddressType
 */
#define AnodeAddress_get_type(a) ((enum AnodeAddressType)((a)->bits[0]))

/**
 * Calculate the short 8 byte address ID from an address
 *
 * @param address Binary address
 * @param short_address_id Buffer to store 8-byte short address ID
 * @return 0 on success or error code on failure
 */
extern int AnodeAddress_calc_short_id(
  const AnodeAddress *address,
  AnodeAddressId *short_address_id);

/**
 * Extract the zone from an anode address
 *
 * @param address Binary address
 * @param zone Zone value-result parameter to fill on success
 * @return 0 on success or error code on failure
 */
extern int AnodeAddress_get_zone(const AnodeAddress *address,AnodeZone *zone);

/**
 * Convert an address to an ASCII string
 *
 * Anode addresses are 64 characters in ASCII form, so the buffer should
 * have 65 bytes of space.
 *
 * @param address Address to convert
 * @param buf Buffer to receive address in string form (should have 65 bytes of space)
 * @param len Length of buffer
 * @return Length of resulting string or a negative error code on error
 */
extern int AnodeAddress_to_string(const AnodeAddress *address,char *buf,int len);

/**
 * Convert a string into an address
 *
 * @param str Address in string form
 * @param address Address buffer to receive result
 * @return Zero on sucess or error code on error
 */
extern int AnodeAddress_from_string(const char *str,AnodeAddress *address);

/**
 * Supported network address types
 */
enum AnodeNetworkAddressType
{
  ANODE_NETWORK_ADDRESS_IPV4 = 0,
  ANODE_NETWORK_ADDRESS_IPV6 = 1,
  ANODE_NETWORK_ADDRESS_ETHERNET = 2,  /* reserved but unused */
  ANODE_NETWORK_ADDRESS_USB = 3,       /* reserved but unused */
  ANODE_NETWORK_ADDRESS_BLUETOOTH = 4, /* reserved but unused */
  ANODE_NETWORK_ADDRESS_IPC = 5,       /* reserved but unused */
  ANODE_NETWORK_ADDRESS_80211S = 6,    /* reserved but unused */
  ANODE_NETWORK_ADDRESS_SERIAL = 7,    /* reserved but unused */
  ANODE_NETWORK_ADDRESS_ANODE_256_40 = 8
};

/**
 * Anode network address
 *
 * This can contain an address of any type: IPv4, IPv6, or Anode, and is used
 * with the common transport API.
 *
 * The length of the address stored in bits[] is determined by the type.
 */
typedef struct
{
  enum AnodeNetworkAddressType type;
  char bits[ANODE_ADDRESS_MAX_LENGTH];
} AnodeNetworkAddress;

/**
 * An endpoint with an address and a port
 */
typedef struct
{
  AnodeNetworkAddress address;
  int port;
} AnodeNetworkEndpoint;

/* Constants for binding to any address (v4 or v6) */
extern const AnodeNetworkAddress AnodeNetworkAddress_IP_ANY_V4;
extern const AnodeNetworkAddress AnodeNetworkAddress_IP_ANY_V6;

/* Local host address in v4 and v6 */
extern const AnodeNetworkAddress AnodeNetworkAddress_IP_LOCAL_V4;
extern const AnodeNetworkAddress AnodeNetworkAddress_IP_LOCAL_V6;

/**
 * Convert a network address to an ASCII string
 *
 * The buffer must have room for a 15 character string for IPv4, a 40 byte
 * string for IPv6, and a 64 byte string for Anode addresses. This does not
 * include the trailing null.
 *
 * @param address Address to convert
 * @param buf Buffer to receive address in string form
 * @param len Length of buffer
 * @return Length of resulting string or a negative error code on error
 */
extern int AnodeNetworkAddress_to_string(const AnodeNetworkAddress *address,char *buf,int len);

/**
 * Convert a string into a network address of the correct type
 *
 * @param str Address in string form
 * @param address Address buffer to receive result
 * @return Zero on sucess or error code on error
 */
extern int AnodeNetworkAddress_from_string(const char *str,AnodeNetworkAddress *address);

/**
 * Fill a network endpoint from a C-API sockaddr structure
 *
 * The argument must be struct sockaddr_in for IPv4 or sockaddr_in6 for IPv6.
 * The common sin_family field will be used to differentiate.
 *
 * @param sockaddr Pointer to proper sockaddr structure
 * @param endpoint Endpoint structure to fill
 * @return Zero on success or error on failure
 */
extern int AnodeNetworkEndpoint_from_sockaddr(const void *sockaddr,AnodeNetworkEndpoint *endpoint);

/**
 * Fill a sockaddr from a network endpoint
 *
 * To support either IPv4 or IPv6 addresses, there is a sockaddr_storage
 * structure in most C APIs. If you supply anything other than an IP address
 * such as an Anode address, this will return an error.
 *
 * @param endpoint Endpoint structure to convert
 * @param sockaddr Sockaddr structure storage
 * @param sockaddr_len Length of sockaddr structure storage in bytes
 * @return Zero on success or error on failure
 */
extern int AnodeNetworkEndpoint_to_sockaddr(const AnodeNetworkEndpoint *endpoint,void *sockaddr,int sockaddr_len);

/* ----------------------------------------------------------------------- */
/* Identity Generation and Management                                      */
/* ----------------------------------------------------------------------- */

/**
 * Anode identity structure containing address and secret key
 *
 * This structure is memcpy-safe, and its members are accessible.
 */
typedef struct
{
  /* The public Anode address */
  AnodeAddress address;

  /* Short address ID */
  AnodeAddressId address_id;

  /* The secret key corresponding with the public address */
  /* Secret length is determined by address type */
  char secret[ANODE_ADDRESS_MAX_SECRET_LENGTH];
} AnodeIdentity;

/**
 * Generate a new identity
 *
 * This generates a public/private key pair and from that generates an
 * identity containing an address and a secret key.
 *
 * @param identity Destination structure to store new identity
 * @param zone Zone ID
 * @param type Type of identity to generate
 * @return Zero on success, error on failure
 */
extern int AnodeIdentity_generate(
  AnodeIdentity *identity,
  const AnodeZone *zone,
  enum AnodeAddressType type);

/**
 * Convert an Anode identity to a string representation
 *
 * @param identity Identity to convert
 * @param dest String buffer
 * @param dest_len Length of string buffer
 * @return Length of string created or negative error code on failure
 */
extern int AnodeIdentity_to_string(
  const AnodeIdentity *identity,
  char *dest,
  int dest_len);

/**
 * Convert a string representation to an Anode identity structure
 *
 * @param identity Destination structure to fill
 * @param str C-string containing string representation
 * @return Zero on success or negative error code on failure
 */
extern int AnodeIdentity_from_string(
  AnodeIdentity *identity,
  const char *str);

/* ----------------------------------------------------------------------- */
/* Transport API                                                           */
/* ----------------------------------------------------------------------- */

struct _AnodeTransport;
typedef struct _AnodeTransport AnodeTransport;
struct _AnodeEvent;
typedef struct _AnodeEvent AnodeEvent;

/**
 * Anode socket
 */
typedef struct
{
  /* Type of socket (read-only) */
  enum {
    ANODE_SOCKET_DATAGRAM = 1,
    ANODE_SOCKET_STREAM_LISTEN = 2,
    ANODE_SOCKET_STREAM_CONNECTION = 3
  } type;

  /* Socket state */
  enum {
    ANODE_SOCKET_CLOSED = 0,
    ANODE_SOCKET_OPEN = 1,
    ANODE_SOCKET_CONNECTING = 2,
  } state;

  /* Local address or remote address for stream connections (read-only) */
  AnodeNetworkEndpoint endpoint;

  /* Name of owning class (read-only) */
  const char *class_name;

  /* Pointers for end user use (writable) */
  void *user_ptr[2];

  /* Special handler to receive events or null for default (writable) */
  void (*event_handler)(const AnodeEvent *event);
} AnodeSocket;

/**
 * Anode transport I/O event
 */
struct _AnodeEvent
{
  enum {
    ANODE_TRANSPORT_EVENT_DATAGRAM_RECEIVED = 1,
    ANODE_TRANSPORT_EVENT_STREAM_INCOMING_CONNECT = 2,
    ANODE_TRANSPORT_EVENT_STREAM_OUTGOING_CONNECT_ESTABLISHED = 3,
    ANODE_TRANSPORT_EVENT_STREAM_OUTGOING_CONNECT_FAILED = 4,
    ANODE_TRANSPORT_EVENT_STREAM_CLOSED = 5,
    ANODE_TRANSPORT_EVENT_STREAM_DATA_RECEIVED = 6,
    ANODE_TRANSPORT_EVENT_STREAM_AVAILABLE_FOR_WRITE = 7,
    ANODE_TRANSPORT_EVENT_DNS_RESULT = 8
  } type;

  AnodeTransport *transport;

  /* Anode socket corresponding to this event */
  AnodeSocket *sock;

  /* Originating endpoint for incoming datagrams */
  AnodeNetworkEndpoint *datagram_from;

  /* DNS lookup results */
  const char *dns_name;
  AnodeNetworkAddress *dns_addresses;
  int dns_address_count;

  /* Error code or 0 for none */
  int error_code;

  /* Data for incoming datagrams and stream received events */
  int data_length;
  char *data;
};

/**
 * Enum used for dns_resolve method in transport to specify query rules
 *
 * This can be specified for ipv4, ipv6, and Anode address types to tell the
 * DNS resolver when to bother querying for addresses of the given type.
 * NEVER means to never query for this type, and ALWAYS means to always
 * query. IF_NO_PREVIOUS means to query for this type if no addresses were
 * found in previous queries. Addresses are queried in the order of ipv4,
 * ipv6, then Anode, so if you specify IF_NO_PREVIOUS for all three you will
 * get addresses in that order of priority.
 */
enum AnodeTransportDnsIncludeMode
{
  ANODE_TRANSPORT_DNS_QUERY_NEVER = 0,
  ANODE_TRANSPORT_DNS_QUERY_ALWAYS = 1,
  ANODE_TRANSPORT_DNS_QUERY_IF_NO_PREVIOUS = 2
};

struct _AnodeTransport
{
  /**
   * Set the default event handler
   *
   * @param transport Transport engine
   * @param event_handler Default event handler
   */
  void (*set_default_event_handler)(AnodeTransport *transport,
    void (*event_handler)(const AnodeEvent *event));

  /**
   * Enqueue a function to be executed during a subsequent call to poll()
   *
   * This can be called from other threads, so it can be used to pass a
   * message to the I/O thread in multithreaded applications.
   *
   * If it is called from the same thread, the function is still queued to be
   * run later rather than being run instantly.
   *
   * The order in which invoked functions are called is undefined.
   *
   * @param transport Transport engine
   * @param ptr Arbitrary pointer to pass to function to be called
   * @param func Function to be called
   */
  void (*invoke)(AnodeTransport *transport,
    void *ptr,
    void (*func)(void *));

  /**
   * Initiate a forward DNS query
   *
   * @param transport Transport instance
   * @param name DNS name to query
   * @param event_handler Event handler or null for default event path
   * @param ipv4_include_mode Inclusion mode for IPv4 addresses
   * @param ipv6_include_mode Inclusion mode for IPv6 addresses
   * @param anode_include_mode Inclusion mode for Anode addresses
   */
  void (*dns_resolve)(AnodeTransport *transport,
    const char *name,
    void (*event_handler)(const AnodeEvent *),
    enum AnodeTransportDnsIncludeMode ipv4_include_mode,
    enum AnodeTransportDnsIncludeMode ipv6_include_mode,
    enum AnodeTransportDnsIncludeMode anode_include_mode);

  /**
   * Open a datagram socket
   *
   * @param transport Transport instance
   * @param local_address Local address to bind
   * @param local_port Local port to bind
   * @param error_code Value-result parameter to receive error code on error
   * @return Listen socket or null if error (check error_code in error case)
   */
  AnodeSocket *(*datagram_listen)(AnodeTransport *transport,
    const AnodeNetworkAddress *local_address,
    int local_port,
    int *error_code);

  /**
   * Open a socket to listen for incoming stream connections
   *
   * @param transport Transport instance
   * @param local_address Local address to bind
   * @param local_port Local port to bind
   * @param error_code Value-result parameter to receive error code on error
   * @return Listen socket or null if error (check error_code in error case)
   */
  AnodeSocket *(*stream_listen)(AnodeTransport *transport,
    const AnodeNetworkAddress *local_address,
    int local_port,
    int *error_code);

  /**
   * Send a datagram to a network endpoint
   *
   * @param transport Transport instance
   * @param socket Originating datagram socket
   * @param data Data to send
   * @param data_len Length of data to send
   * @param to_endpoint Destination endpoint
   * @return Zero on success or error code on error
   */
  int (*datagram_send)(AnodeTransport *transport,
    AnodeSocket *sock,
    const void *data,
    int data_len,
    const AnodeNetworkEndpoint *to_endpoint);

  /**
   * Initiate an outgoing stream connection attempt
   *
   * For IPv4 and IPv6 addresses, this will initiate a TCP connection. For
   * Anode addresses, Anode's internal streaming protocol will be used.
   *
   * @param transport Transport instance
   * @param to_endpoint Destination endpoint
   * @param error_code Error code value-result parameter, filled on error
   * @return Stream socket object or null on error (check error_code)
   */
  AnodeSocket *(*stream_connect)(AnodeTransport *transport,
    const AnodeNetworkEndpoint *to_endpoint,
    int *error_code);

  /**
   * Indicate that you are interested in writing to a stream
   *
   * This does nothing if the socket is not a stream connection or is not
   * connected.
   *
   * @param transport Transport instance
   * @param sock Stream connection
   */
  void (*stream_start_writing)(AnodeTransport *transport,
    AnodeSocket *sock);

  /**
   * Indicate that you are no longer interested in writing to a stream
   *
   * This does nothing if the socket is not a stream connection or is not
   * connected.
   *
   * @param transport Transport instance
   * @param sock Stream connection
   */
  void (*stream_stop_writing)(AnodeTransport *transport,
    AnodeSocket *sock);

  /**
   * Send data to a stream connection
   *
   * This must be called after a stream is indicated to be ready for writing.
   * It returns the number of bytes actually written, or a negative error
   * code on failure.
   *
   * A return value of zero can occur here, and simply indicates that nothing
   * was sent. This may occur with certain network stacks on certain
   * platforms.
   *
   * @param transport Transport engine
   * @param sock Stream socket
   * @param data Data to send
   * @param data_len Maximum data to send in bytes
   * @return Actual data sent or negative error code on error
   */
  int (*stream_send)(AnodeTransport *transport,
    AnodeSocket *sock,
    const void *data,
    int data_len);

  /**
   * Close a socket
   *
   * If the socket is a stream connection in the connected state, this
   * will generate a stream closed event with a zero error_code to indicate
   * a normal close.
   *
   * @param transport Transport engine
   * @param sock Socket object
   */
  void (*close)(AnodeTransport *transport,
    AnodeSocket *sock);

  /**
   * Run main polling loop
   *
   * This should be called repeatedly from the I/O thread of your main
   * process. It blocks until one or more events occur, and then returns
   * the number of events. Error returns here are fatal and indicate
   * serious problems such as build or platform issues or a lack of any
   * network interface.
   *
   * Functions queued with invoke() are also called inside here.
   *
   * @param transport Transport engine
   * @return Number of events handled or negative on (fatal) error
   */
  int (*poll)(AnodeTransport *transport);

  /**
   * Check whether transport supports an address type
   *
   * Inheriting classes should call their base if they do not natively
   * speak the specified type.
   *
   * @param transport Transport engine
   * @param at Address type
   * @return Nonzero if true
   */
  int (*supports_address_type)(const AnodeTransport *transport,
    enum AnodeNetworkAddressType at);

  /**
   * Get the instance of AnodeTransport under this one (if any)
   *
   * @param transport Transport engine
   * @return Base instance or null if none
   */
  AnodeTransport *(*base_instance)(const AnodeTransport *transport);

  /**
   * @param transport Transport engine
   * @return Class name of this instance
   */
  const char *(*class_name)(AnodeTransport *transport);

  /**
   * Delete this transport and its base transports
   *
   * The 'transport' pointer and any streams or sockets it owns are no longer
   * valid after this call.
   *
   * @param transport Transport engine
   */
  void (*delete)(AnodeTransport *transport);
};

/**
 * Construct a new system transport
 *
 * This is the default base for AnodeTransport, and it is constructed
 * automatically if 'base' is null in AnodeTransport_new(). However, it also
 * exposed to the user so that specialized transports (such as those that use
 * proxy servers) can be developed on top of it. These in turn can be supplied
 * as 'base' to AnodeTransport_new() to talk Anode over these transports.
 *
 * The system transport supports IP protocols and possibly others.
 *
 * @param base Base class or null for none (usually null)
 * @return Base transport engine instance
 */
extern AnodeTransport *AnodeSystemTransport_new(AnodeTransport *base);

/**
 * Construct a new Anode core transport
 *
 * This is the transport that talks Anode using the specified base transport.
 * Requests for other address types are passed through to the base. If the
 * base is null, an instance of AnodeSystemTransport is used.
 *
 * Since transport engines inherit their functionality, this transport
 * will also do standard IP and everything else that the system transport
 * supports. Most users will just want to construct this with a null base.
 *
 * @param base Base transport to use, or null to use SystemTransport
 * @return Anode transport engine or null on error
 */
extern AnodeTransport *AnodeCoreTransport_new(AnodeTransport *base);

/* ----------------------------------------------------------------------- */
/* URI Parser                                                              */
/* ----------------------------------------------------------------------- */

/**
 * URI broken down by component
 */
typedef struct
{
  char scheme[8];
  char username[64];
  char password[64];
  char host[128];
  char path[256];
  char query[256];
  char fragment[64];
  int port;
} AnodeURI;

/**
 * URI parser
 *
 * A buffer too small error will occur if any field is too large for the
 * AnodeURI structure.
 *
 * @param parsed_uri Structure to fill with parsed URI data
 * @param uri_string URI in string format
 * @return Zero on success or error on failure
 */
extern int AnodeURI_parse(AnodeURI *parsed_uri,const char *uri_string);

/**
 * Output a URI in string format
 *
 * @param uri URI to output as string
 * @param buf Buffer to store URI string
 * @param len Length of buffer
 * @return Buffer or null on error
 */
extern char *AnodeURI_to_string(const AnodeURI *uri,char *buf,int len);

/* ----------------------------------------------------------------------- */
/* Zone File Lookup and Dictionary                                         */
/* ----------------------------------------------------------------------- */

/**
 * Zone file dictionary
 */
typedef void AnodeZoneFile;

/**
 * Start asynchronous zone fetch
 *
 * When the zone is retrieved, the lookup handler is called. If zone lookup
 * failed, the zone file argument to the handler will be null.
 *
 * @param transport Transport engine
 * @param zone Zone ID
 * @param user_ptr User pointer
 * @param zone_lookup_handler Handler for Anode zone lookup
 */
extern void AnodeZoneFile_lookup(
  AnodeTransport *transport,
  const AnodeZone *zone,
  void *ptr,
  void (*zone_lookup_handler)(const AnodeZone *,AnodeZoneFile *,void *));

/**
 * Look up a key in a zone file
 *
 * @param zone Zone file object
 * @param key Key to get in zone file
 */
extern const char *AnodeZoneFile_get(const AnodeZoneFile *zone,const char *key);

/**
 * Free a zone file
 *
 * @param zone Zone to free
 */
extern void AnodeZoneFile_free(AnodeZoneFile *zone);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif
