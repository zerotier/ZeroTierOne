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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include "../anode.h"
#include "../misc.h"
#include "../http_client.h"
#include "../dictionary.h"

struct TestCase
{
  int method;
  AnodeURI uri;
  const void *client_data;
  unsigned int client_data_len;
  const char *expected_sha1;
  char actual_sha1[64];
  int got_it;
  int keepalive;
  struct TestCase *next;
};

#define NUM_TEST_CASES 7
static struct TestCase test_cases[NUM_TEST_CASES];

static void init_test_cases(int keepalive)
{
  AnodeURI_parse(&(test_cases[0].uri),"http://zerotier.com/for_unit_tests/test1.txt");
  test_cases[0].method = ANODE_HTTP_GET;
  test_cases[0].client_data_len = 0;
  test_cases[0].expected_sha1 = "0828324174b10cc867b7255a84a8155cf89e1b8b";
  test_cases[0].actual_sha1[0] = (char)0;
  test_cases[0].got_it = 0;
  test_cases[0].keepalive = keepalive;
  test_cases[0].next = &(test_cases[1]);

  AnodeURI_parse(&(test_cases[1].uri),"http://zerotier.com/for_unit_tests/test2.bin");
  test_cases[1].method = ANODE_HTTP_GET;
  test_cases[1].client_data_len = 0;
  test_cases[1].expected_sha1 = "6b67c635786ab52666211d02412c0d0f0372980d";
  test_cases[1].actual_sha1[0] = (char)0;
  test_cases[1].got_it = 0;
  test_cases[1].keepalive = keepalive;
  test_cases[1].next = &(test_cases[2]);

  AnodeURI_parse(&(test_cases[2].uri),"http://zerotier.com/for_unit_tests/test3.bin");
  test_cases[2].method = ANODE_HTTP_GET;
  test_cases[2].client_data_len = 0;
  test_cases[2].expected_sha1 = "efa7722029fdbb6abd0e3ed32a0b44bfb982cff0";
  test_cases[2].actual_sha1[0] = (char)0;
  test_cases[2].got_it = 0;
  test_cases[2].keepalive = keepalive;
  test_cases[2].next = &(test_cases[3]);

  AnodeURI_parse(&(test_cases[3].uri),"http://zerotier.com/for_unit_tests/test4.bin");
  test_cases[3].method = ANODE_HTTP_GET;
  test_cases[3].client_data_len = 0;
  test_cases[3].expected_sha1 = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
  test_cases[3].actual_sha1[0] = (char)0;
  test_cases[3].got_it = 0;
  test_cases[3].keepalive = keepalive;
  test_cases[3].next = &(test_cases[4]);

  AnodeURI_parse(&(test_cases[4].uri),"http://zerotier.com/for_unit_tests/echo.php?echo=foobar");
  test_cases[4].method = ANODE_HTTP_GET;
  test_cases[4].client_data_len = 0;
  test_cases[4].expected_sha1 = "8843d7f92416211de9ebb963ff4ce28125932878";
  test_cases[4].actual_sha1[0] = (char)0;
  test_cases[4].got_it = 0;
  test_cases[4].keepalive = keepalive;
  test_cases[4].next = &(test_cases[5]);

  AnodeURI_parse(&(test_cases[5].uri),"http://zerotier.com/for_unit_tests/echo.php");
  test_cases[5].method = ANODE_HTTP_POST;
  test_cases[5].client_data = "echo=foobar";
  test_cases[5].client_data_len = strlen((char *)test_cases[5].client_data);
  test_cases[5].expected_sha1 = "8843d7f92416211de9ebb963ff4ce28125932878";
  test_cases[5].actual_sha1[0] = (char)0;
  test_cases[5].got_it = 0;
  test_cases[5].keepalive = keepalive;
  test_cases[5].next = &(test_cases[6]);

  AnodeURI_parse(&(test_cases[6].uri),"http://zerotier.com/for_unit_tests/test3.bin");
  test_cases[6].method = ANODE_HTTP_HEAD;
  test_cases[6].client_data_len = 0;
  test_cases[6].expected_sha1 = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
  test_cases[6].actual_sha1[0] = (char)0;
  test_cases[6].got_it = 0;
  test_cases[6].keepalive = keepalive;
  test_cases[6].next = 0;
}

static int http_handler_dump_headers(void *arg,const char *key,const char *value)
{
  printf(" H %s: %s\n",key,value);
  return 1;
}

static void http_handler(struct AnodeHttpClient *client)
{
  const char *method = "???";
  char buf[1024];
  unsigned char sha[20];
  struct TestCase *test = (struct TestCase *)client->ptr[0];

  switch(client->method) {
    case ANODE_HTTP_GET:
      method = "GET";
      break;
    case ANODE_HTTP_HEAD:
      method = "HEAD";
      break;
    case ANODE_HTTP_POST:
      method = "POST";
      break;
  }

  if (client->response.code == 200) {
    SHA1((unsigned char *)client->response.data,client->response.data_length,sha);
    Anode_to_hex(sha,20,test->actual_sha1,sizeof(test->actual_sha1));
    printf("%s %s\n * SHA1: %s exp: %s\n",method,AnodeURI_to_string(&(test->uri),buf,sizeof(buf)),test->actual_sha1,test->expected_sha1);
    if (strcmp(test->actual_sha1,test->expected_sha1))
      printf(" ! SHA1 MISMATCH!\n");
    AnodeDictionary_iterate(&(client->response.headers),0,&http_handler_dump_headers);
  } else printf("%s %s: ERROR: %d\n",method,AnodeURI_to_string(&(test->uri),buf,sizeof(buf)),client->response.code);

  test->got_it = 1;

  if (!test->keepalive)
    AnodeHttpClient_free(client);
  else {
    test = test->next;
    if (test) {
      memcpy((void *)&(client->uri),(const void *)&(test->uri),sizeof(AnodeURI));

      client->data = test->client_data;
      client->data_length = test->client_data_len;
      client->ptr[0] = test;
      client->keepalive = test->keepalive;
      client->method = test->method;
      client->handler = &http_handler;

      AnodeHttpClient_send(client);
    } else {
      AnodeHttpClient_free(client);
    }
  }
}

int main(int argc,char **argv)
{
  struct AnodeHttpClient *client;
  AnodeTransportEngine transport_engine;
  int i;

  if (Anode_init_ip_transport_engine(&transport_engine)) {
    printf("Failed (transport engine init)\n");
    return 1;
  }

  printf("Testing without keepalive...\n\n");
  init_test_cases(0);
  for(i=0;i<NUM_TEST_CASES;++i) {
    client = AnodeHttpClient_new(&transport_engine);

    memcpy((void *)&(client->uri),(const void *)&(test_cases[i].uri),sizeof(AnodeURI));
    client->data = test_cases[i].client_data;
    client->data_length = test_cases[i].client_data_len;
    client->ptr[0] = &test_cases[i];
    client->keepalive = test_cases[i].keepalive;
    client->method = test_cases[i].method;
    client->handler = &http_handler;

    AnodeHttpClient_send(client);
  }

  for(;;) {
    for(i=0;i<NUM_TEST_CASES;++i) {
      if (!test_cases[i].got_it)
        break;
    }
    if (i == NUM_TEST_CASES)
      break;
    transport_engine.poll(&transport_engine);
  }
  printf("\n\n");

  printf("Testing with keepalive...\n\n");
  init_test_cases(1);

  client = AnodeHttpClient_new(&transport_engine);

  i = 0;
  memcpy((void *)&(client->uri),(const void *)&(test_cases[i].uri),sizeof(AnodeURI));
  client->data = test_cases[i].client_data;
  client->data_length = test_cases[i].client_data_len;
  client->ptr[0] = &test_cases[i];
  client->keepalive = test_cases[i].keepalive;
  client->method = test_cases[i].method;
  client->handler = &http_handler;

  AnodeHttpClient_send(client);

  for(;;) {
    for(i=0;i<NUM_TEST_CASES;++i) {
      if (!test_cases[i].got_it)
        break;
    }
    if (i == NUM_TEST_CASES)
      break;
    transport_engine.poll(&transport_engine);
  }

  transport_engine.destroy(&transport_engine);

  return 0;
}
