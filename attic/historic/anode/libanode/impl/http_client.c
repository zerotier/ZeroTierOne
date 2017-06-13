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

#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "http_client.h"
#include "misc.h"
#include "types.h"

/* How much to increment read buffer at each capacity top? */
#define ANODE_HTTP_CAPACITY_INCREMENT 4096

static void AnodeHttpClient_close_and_fail(struct AnodeHttpClient *client)
{
  if (client->impl.tcp_connection) {
    client->impl.transport_engine->tcp_close(client->impl.transport_engine,client->impl.tcp_connection);
    client->impl.tcp_connection = (AnodeTransportTcpConnection *)0;
  }

  client->response.data_length = 0;
  client->impl.phase = ANODE_HTTP_REQUEST_PHASE_CLOSED;

  if (client->handler)
    client->handler(client);
}

static void AnodeHttpClient_do_initiate_client(struct AnodeHttpClient *client)
{
  const char *method = "";
  long l,i;

  switch(client->method) {
    case ANODE_HTTP_GET: method = "GET"; break;
    case ANODE_HTTP_HEAD: method = "HEAD"; break;
    case ANODE_HTTP_POST: method = "POST"; break;
  }
  client->impl.outbuf_len = snprintf((char *)client->impl.outbuf,sizeof(client->impl.outbuf),
    "%s %s%s%s HTTP/1.1\r\nHost: %s:%d\r\n%s",
    method,
    client->uri.path,
    ((client->uri.query[0]) ? "?" : ""),
    client->uri.query,
    client->uri.host,
    ((client->uri.port > 0) ? client->uri.port : 80),
    ((client->keepalive) ? "" : "Connection: close\r\n")
  );
  if (client->impl.outbuf_len >= (sizeof(client->impl.outbuf) - 2)) {
    client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_HEADERS_TOO_LARGE;
    AnodeHttpClient_close_and_fail(client);
    return;
  }

  if (client->method == ANODE_HTTP_POST) {
    if ((client->data)&&(client->data_length)) {
      client->impl.outbuf_len += snprintf((char *)client->impl.outbuf + client->impl.outbuf_len,sizeof(client->impl.outbuf) - client->impl.outbuf_len,
        "Content-Type: %s\r\n",
        (client->data_content_type ? client->data_content_type : "application/x-www-form-urlencoded")
      );
      if (client->impl.outbuf_len >= (sizeof(client->impl.outbuf) - 2)) {
        client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_HEADERS_TOO_LARGE;
        AnodeHttpClient_close_and_fail(client);
        return;
      }
      client->impl.outbuf_len += snprintf((char *)client->impl.outbuf + client->impl.outbuf_len,sizeof(client->impl.outbuf) - client->impl.outbuf_len,
        "Content-Length: %u\r\n",
        client->data_length
      );
      if (client->impl.outbuf_len >= (sizeof(client->impl.outbuf) - 2)) {
        client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_HEADERS_TOO_LARGE;
        AnodeHttpClient_close_and_fail(client);
        return;
      }
    } else {
      client->impl.outbuf_len += snprintf((char *)client->impl.outbuf + client->impl.outbuf_len,sizeof(client->impl.outbuf) - client->impl.outbuf_len,
        "Content-Length: 0\r\n"
      );
      if (client->impl.outbuf_len >= (sizeof(client->impl.outbuf) - 2)) {
        client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_HEADERS_TOO_LARGE;
        AnodeHttpClient_close_and_fail(client);
        return;
      }
    }
  }

  l = AnodeDictionary_write(&(client->headers),(char *)client->impl.outbuf + client->impl.outbuf_len,(long)(sizeof(client->impl.outbuf) - client->impl.outbuf_len - 2),"\r\n",": ");
  if (l < 0) {
    client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_HEADERS_TOO_LARGE;
    AnodeHttpClient_close_and_fail(client);
    return;
  }

  client->impl.outbuf_len += (unsigned int)l;
  if (client->impl.outbuf_len >= (sizeof(client->impl.outbuf) - 2)) { /* sanity check */
    client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_HEADERS_TOO_LARGE;
    AnodeHttpClient_close_and_fail(client);
    return;
  }

  client->impl.outbuf[client->impl.outbuf_len++] = '\r';
  client->impl.outbuf[client->impl.outbuf_len++] = '\n';

  if ((client->method == ANODE_HTTP_POST)&&(client->data)&&(client->data_length)) {
    i = sizeof(client->impl.outbuf) - client->impl.outbuf_len;
    if (i > client->data_length)
      i = client->data_length;
    Anode_memcpy((client->impl.outbuf + client->impl.outbuf_len),client->data,i);
    client->impl.request_data_ptr += i;
    client->impl.outbuf_len += i;
  }

  client->impl.phase = ANODE_HTTP_REQUEST_PHASE_SEND;
  client->impl.transport_engine->tcp_start_writing(client->impl.transport_engine,client->impl.tcp_connection);
}

static void AnodeHttpClient_tcp_outgoing_connect_handler(
  AnodeTransportEngine *transport,
  AnodeTransportTcpConnection *connection,
  int error_code)
{
  struct AnodeHttpClient *client;

  if (!(client = (struct AnodeHttpClient *)(connection->ptr)))
    return;

  if ((client->impl.phase == ANODE_HTTP_REQUEST_PHASE_CONNECT)&&(!client->impl.freed)) {
    if (error_code) {
      client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_CONNECT_FAILED;
      AnodeHttpClient_close_and_fail(client);
    } else {
      client->impl.tcp_connection = connection;
      AnodeHttpClient_do_initiate_client(client);
    }
  } else transport->tcp_close(transport,connection);
}

static void AnodeHttpClient_tcp_connection_terminated_handler(
  AnodeTransportEngine *transport,
  AnodeTransportTcpConnection *connection,
  int error_code)
{
  struct AnodeHttpClient *client;

  if (!(client = (struct AnodeHttpClient *)(connection->ptr)))
    return;
  if (client->impl.freed)
    return;

  client->response.data_length = 0;
  client->impl.tcp_connection = (AnodeTransportTcpConnection *)0;
  if ((client->impl.phase != ANODE_HTTP_REQUEST_PHASE_KEEPALIVE)&&(client->impl.phase != ANODE_HTTP_REQUEST_PHASE_CLOSED)) {
    client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_SERVER_CLOSED_CONNECTION;
    client->impl.phase = ANODE_HTTP_REQUEST_PHASE_CLOSED;
    AnodeHttpClient_close_and_fail(client);
  } else client->impl.phase = ANODE_HTTP_REQUEST_PHASE_CLOSED;
}

static void AnodeHttpClient_tcp_receive_handler(
  AnodeTransportEngine *transport,
  AnodeTransportTcpConnection *connection,
  void *data,
  unsigned int data_length)
{
  struct AnodeHttpClient *client;
  char *p1,*p2;
  unsigned int i;
  long l;

  if (!(client = (struct AnodeHttpClient *)(connection->ptr)))
    return;
  if (client->impl.freed) {
    transport->tcp_close(transport,connection);
    return;
  }

  if (!client->response.data)
    client->response.data = malloc(client->impl.response_data_capacity = ANODE_HTTP_CAPACITY_INCREMENT);

  i = 0;
  while (i < data_length) {
    switch(client->impl.read_mode) {
      case ANODE_HTTP_READ_MODE_WAITING:
        for(;i<data_length;++i) {
          if (((const char *)data)[i] == '\n') {
            ((char *)client->response.data)[client->response.data_length] = (char)0;
            client->response.data_length = 0;

            p1 = (char *)Anode_strchr((char *)client->response.data,' ');
            if (!p1)
              p1 = (char *)Anode_strchr((char *)client->response.data,'\t');
            if (p1) {
              while ((*p1 == ' ')||(*p1 == '\t')) ++p1;
              if (!*p1) {
                client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_INVALID_RESPONSE;
                AnodeHttpClient_close_and_fail(client);
                return;
              }
              p2 = p1 + 1;
              while (*p2) {
                if ((*p2 == ' ')||(*p2 == '\t')||(*p2 == '\r')||(*p2 == '\n')) {
                  *p2 = (char)0;
                  break;
                } else ++p2;
              }
              client->response.code = (int)strtol(p1,(char **)0,10);
              client->impl.read_mode = ANODE_HTTP_READ_MODE_HEADERS;
              ++i; break; /* Exit inner for() */
            }
          } else {
            ((char *)client->response.data)[client->response.data_length++] = ((const char *)data)[i];
            if (client->response.data_length >= client->impl.response_data_capacity)
              client->response.data = realloc(client->response.data,client->impl.response_data_capacity += ANODE_HTTP_CAPACITY_INCREMENT);
          }
        }
        break;
      case ANODE_HTTP_READ_MODE_HEADERS:
      case ANODE_HTTP_READ_MODE_CHUNKED_FOOTER:
        for(;i<data_length;++i) {
          if (((const char *)data)[i] == '\n') {
            client->impl.header_line_buf[client->impl.header_line_buf_ptr] = (char)0;
            client->impl.header_line_buf_ptr = 0;

            if ((!client->impl.header_line_buf[0])||((client->impl.header_line_buf[0] == '\r')&&(!client->impl.header_line_buf[1]))) {
              /* If the line is empty (or is empty with \r\n as the
               * line terminator), we're at the end. */
              if (client->impl.read_mode == ANODE_HTTP_READ_MODE_CHUNKED_FOOTER) {
                /* If this is a chunked footer, we finally end the
                 * chunked response. */
                client->impl.read_mode = ANODE_HTTP_READ_MODE_WAITING;
                if (client->keepalive)
                  client->impl.phase = ANODE_HTTP_REQUEST_PHASE_KEEPALIVE;
                else {
                  client->impl.transport_engine->tcp_close(client->impl.transport_engine,client->impl.tcp_connection);
                  client->impl.tcp_connection = (AnodeTransportTcpConnection *)0;
                  client->impl.phase = ANODE_HTTP_REQUEST_PHASE_CLOSED;
                }
                if (client->handler)
                  client->handler(client);
                if (client->impl.freed)
                  return;
              } else {
                /* Otherwise, this is a regular header block */
                if (client->response.code == 100) {
                  /* Ignore 100 Continue messages */
                  client->impl.read_mode = ANODE_HTTP_READ_MODE_WAITING;
                  ++i; break; /* Exit inner for() */
                } else if ((client->response.code == 200)&&(client->method != ANODE_HTTP_HEAD)) {
                  /* Other messages get their headers parsed to determine
                   * how to read them. */
                  p1 = (char *)AnodeDictionary_get(&(client->response.headers),"transfer-encoding");
                  if ((p1)&&(Anode_strcaseeq(p1,"chunked"))) {
                    /* Chunked encoding enters chunked mode */
                    client->impl.header_line_buf_ptr = 0;
                    client->impl.read_mode = ANODE_HTTP_READ_MODE_CHUNKED_CHUNK_SIZE;
                    ++i; break; /* Exit inner for() */
                  } else {
                    /* Else we must have a Content-Length header */
                    p1 = (char *)AnodeDictionary_get(&(client->response.headers),"content-length");
                    if (!p1) {
                      /* No chunked or content length is not supported */
                      client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_INVALID_RESPONSE;
                      AnodeHttpClient_close_and_fail(client);
                      return;
                    } else {
                      /* Enter block read mode with content length */
                      l = strtol(p1,(char **)0,10);
                      if (l <= 0) {
                        /* Zero length data is all done... */
                        client->impl.expecting_response_length = 0;
                        client->impl.read_mode = ANODE_HTTP_READ_MODE_WAITING;
                        if (client->keepalive)
                          client->impl.phase = ANODE_HTTP_REQUEST_PHASE_KEEPALIVE;
                        else {
                          client->impl.transport_engine->tcp_close(client->impl.transport_engine,client->impl.tcp_connection);
                          client->impl.tcp_connection = (AnodeTransportTcpConnection *)0;
                          client->impl.phase = ANODE_HTTP_REQUEST_PHASE_CLOSED;
                        }

                        if (client->handler)
                          client->handler(client);
                        if (client->impl.freed)
                          return;

                        ++i; break; /* Exit inner for() */
                      } else {
                        /* Else start reading... */
                        client->impl.expecting_response_length = (unsigned int)l;
                        client->impl.read_mode = ANODE_HTTP_READ_MODE_BLOCK;
                        ++i; break; /* Exit inner for() */
                      }
                    }
                  }
                } else {
                  /* HEAD clients or non-200 codes get headers only */
                  client->impl.expecting_response_length = 0;
                  client->impl.read_mode = ANODE_HTTP_READ_MODE_WAITING;
                  if (client->keepalive)
                    client->impl.phase = ANODE_HTTP_REQUEST_PHASE_KEEPALIVE;
                  else {
                    client->impl.transport_engine->tcp_close(client->impl.transport_engine,client->impl.tcp_connection);
                    client->impl.tcp_connection = (AnodeTransportTcpConnection *)0;
                    client->impl.phase = ANODE_HTTP_REQUEST_PHASE_CLOSED;
                  }

                  if (client->handler)
                    client->handler(client);
                  if (client->impl.freed)
                    return;

                  ++i; break; /* Exit inner for() */
                }
              }
            } else {
              /* Otherwise this is another header, add to dictionary */
              AnodeDictionary_read(
                &(client->response.headers),
                client->impl.header_line_buf,
                "\r\n",
                ": \t",
                "",
                (char)0,
                1,
                1
              );
            }
          } else {
            client->impl.header_line_buf[client->impl.header_line_buf_ptr++] = ((const char *)data)[i];
            if (client->impl.header_line_buf_ptr >= sizeof(client->impl.header_line_buf)) {
              client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_INVALID_RESPONSE;
              AnodeHttpClient_close_and_fail(client);
              return;
            }
          }
        }
        break;
      case ANODE_HTTP_READ_MODE_BLOCK:
        if ((client->response.data_length + client->impl.expecting_response_length) > client->impl.response_data_capacity)
          client->response.data = realloc(client->response.data,client->impl.response_data_capacity = (client->response.data_length + client->impl.expecting_response_length));

        for(;((i<data_length)&&(client->impl.expecting_response_length));++i) {
          ((char *)client->response.data)[client->response.data_length++] = ((const char *)data)[i];
          --client->impl.expecting_response_length;
        }

        if (!client->impl.expecting_response_length) {
          client->impl.read_mode = ANODE_HTTP_READ_MODE_WAITING;
          if (client->keepalive)
            client->impl.phase = ANODE_HTTP_REQUEST_PHASE_KEEPALIVE;
          else {
            client->impl.transport_engine->tcp_close(client->impl.transport_engine,client->impl.tcp_connection);
            client->impl.tcp_connection = (AnodeTransportTcpConnection *)0;
            client->impl.phase = ANODE_HTTP_REQUEST_PHASE_CLOSED;
          }

          if (client->handler)
            client->handler(client);
          if (client->impl.freed)
            return;
        }
        break;
      case ANODE_HTTP_READ_MODE_CHUNKED_CHUNK_SIZE:
        for(;i<data_length;++i) {
          if (((const char *)data)[i] == '\n') {
            client->impl.header_line_buf[client->impl.header_line_buf_ptr] = (char)0;
            client->impl.header_line_buf_ptr = 0;

            p1 = client->impl.header_line_buf;
            while (*p1) {
              if ((*p1 == ';')||(*p1 == ' ')||(*p1 == '\r')||(*p1 == '\n')||(*p1 == '\t')) {
                *p1 = (char)0;
                break;
              } else ++p1;
            }

            if (client->impl.header_line_buf[0]) {
              l = strtol(client->impl.header_line_buf,(char **)0,16);
              if (l <= 0) {
                /* Zero length ends chunked and enters footer mode */
                client->impl.expecting_response_length = 0;
                client->impl.read_mode = ANODE_HTTP_READ_MODE_CHUNKED_FOOTER;
              } else {
                /* Otherwise the next chunk is to be read */
                client->impl.expecting_response_length = (unsigned int)l;
                client->impl.read_mode = ANODE_HTTP_READ_MODE_CHUNKED_DATA;
              }
              ++i; break; /* Exit inner for() */
            }
          } else {
            client->impl.header_line_buf[client->impl.header_line_buf_ptr++] = ((const char *)data)[i];
            if (client->impl.header_line_buf_ptr >= sizeof(client->impl.header_line_buf)) {
              client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_INVALID_RESPONSE;
              AnodeHttpClient_close_and_fail(client);
              return;
            }
          }
        }
        break;
      case ANODE_HTTP_READ_MODE_CHUNKED_DATA:
        if ((client->response.data_length + client->impl.expecting_response_length) > client->impl.response_data_capacity)
          client->response.data = realloc(client->response.data,client->impl.response_data_capacity = (client->response.data_length + client->impl.expecting_response_length));

        for(;((i<data_length)&&(client->impl.expecting_response_length));++i) {
          ((char *)client->response.data)[client->response.data_length++] = ((const char *)data)[i];
          --client->impl.expecting_response_length;
        }

        if (!client->impl.expecting_response_length)
          client->impl.read_mode = ANODE_HTTP_READ_MODE_CHUNKED_CHUNK_SIZE;
        break;
    }
  }
}

static void AnodeHttpClient_tcp_available_for_write_handler(
  AnodeTransportEngine *transport,
  AnodeTransportTcpConnection *connection)
{
  struct AnodeHttpClient *client;
  unsigned int i,j;
  int n;

  if (!(client = (struct AnodeHttpClient *)(connection->ptr)))
    return;
  if (client->impl.freed) {
    transport->tcp_close(transport,connection);
    return;
  }

  if (client->impl.phase == ANODE_HTTP_REQUEST_PHASE_SEND) {
    n = client->impl.transport_engine->tcp_send(client->impl.transport_engine,client->impl.tcp_connection,(const void *)client->impl.outbuf,(int)client->impl.outbuf_len);
    if (n < 0) {
      client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_SERVER_CLOSED_CONNECTION;
      AnodeHttpClient_close_and_fail(client);
    } else if (n > 0) {
      for(i=0,j=(client->impl.outbuf_len - (unsigned int)n);i<j;++i)
        client->impl.outbuf[i] = client->impl.outbuf[i + (unsigned int)n];
      client->impl.outbuf_len -= (unsigned int)n;

      if ((client->method == ANODE_HTTP_POST)&&(client->data)&&(client->data_length)) {
        i = sizeof(client->impl.outbuf) - client->impl.outbuf_len;
        j = client->data_length - client->impl.request_data_ptr;
        if (i > j)
          i = j;
        Anode_memcpy((client->impl.outbuf + client->impl.outbuf_len),client->data,i);
        client->impl.request_data_ptr += i;
        client->impl.outbuf_len += i;
      }

      if (!client->impl.outbuf_len) {
        client->impl.transport_engine->tcp_stop_writing(client->impl.transport_engine,client->impl.tcp_connection);
        client->impl.phase = ANODE_HTTP_REQUEST_PHASE_RECEIVE;
      }
    }
  } else client->impl.transport_engine->tcp_stop_writing(client->impl.transport_engine,client->impl.tcp_connection);
}

static void AnodeHttpClient_dns_result_handler(
  AnodeTransportEngine *transport,
  void *ptr,
  int error_code,
  const char *name,
  const AnodeTransportIpAddress *ip_addresses,
  unsigned int ip_address_count,
  const AnodeAddress *anode_address)
{
  struct AnodeHttpClient *client;
  AnodeTransportIpEndpoint to_endpoint;

  if (!(client = (struct AnodeHttpClient *)ptr))
    return;
  if (client->impl.freed)
    return;

  if ((error_code)||(!ip_address_count)) {
    if (client->impl.phase == ANODE_HTTP_REQUEST_PHASE_RESOLVE) {
      client->response.code = ANODE_HTTP_SPECIAL_RESPONSE_DNS_RESOLVE_FAILED;
      AnodeHttpClient_close_and_fail(client);
    }
  } else {
    client->impl.phase = ANODE_HTTP_REQUEST_PHASE_CONNECT;
    Anode_memcpy(&to_endpoint.address,ip_addresses,sizeof(AnodeTransportIpAddress));
    to_endpoint.port = (client->uri.port > 0) ? client->uri.port : 80;
    client->impl.transport_engine->tcp_connect(
      client->impl.transport_engine,
      client,
      &AnodeHttpClient_tcp_outgoing_connect_handler,
      &AnodeHttpClient_tcp_connection_terminated_handler,
      &AnodeHttpClient_tcp_receive_handler,
      &AnodeHttpClient_tcp_available_for_write_handler,
      &to_endpoint);
  }
}

struct AnodeHttpClient *AnodeHttpClient_new(AnodeTransportEngine *transport_engine)
{
  struct AnodeHttpClient *req = malloc(sizeof(struct AnodeHttpClient));
  Anode_zero(req,sizeof(struct AnodeHttpClient));

  AnodeDictionary_init(&(req->headers),0);
  AnodeDictionary_init(&(req->response.headers),0);

  req->impl.transport_engine = transport_engine;

  return req;
}

void AnodeHttpClient_send(struct AnodeHttpClient *client)
{
  client->response.code = 0;
  client->response.data_length = 0;
  AnodeDictionary_clear(&(client->response.headers));

  client->impl.request_data_ptr = 0;
  client->impl.expecting_response_length = 0;
  client->impl.read_mode = ANODE_HTTP_READ_MODE_WAITING;
  client->impl.outbuf_len = 0;

  if (!client->impl.tcp_connection) {
    client->impl.transport_engine->dns_resolve(
      client->impl.transport_engine,
      &AnodeHttpClient_dns_result_handler,
      client,
      client->uri.host,
      ANODE_TRANSPORT_DNS_QUERY_ALWAYS,
      ANODE_TRANSPORT_DNS_QUERY_IF_NO_PREVIOUS,
      ANODE_TRANSPORT_DNS_QUERY_NEVER);
  } else AnodeHttpClient_do_initiate_client(client);
}

void AnodeHttpClient_free(struct AnodeHttpClient *client)
{
  AnodeDictionary_destroy(&(client->headers));
  AnodeDictionary_destroy(&(client->response.headers));

  if (client->impl.tcp_connection) {
    client->impl.transport_engine->tcp_close(client->impl.transport_engine,client->impl.tcp_connection);
    client->impl.tcp_connection = (AnodeTransportTcpConnection *)0;
  }

  if (client->response.data)
    free(client->response.data);

  client->impl.freed = 1;
  client->impl.transport_engine->run_later(client->impl.transport_engine,client,&free);
}
