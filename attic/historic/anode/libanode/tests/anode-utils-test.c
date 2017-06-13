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
#include "../anode.h"
#include "../misc.h"

static const char *testuris[22] = {
  "http://www.test.com",
  "http://www.test.com/",
  "http://www.test.com/path/to/something",
  "http://user@www.test.com",
  "http://user@www.test.com/path/to/something",
  "http://user:password@www.test.com/path/to/something",
  "http://www.test.com/path/to/something?query=foo&bar=baz",
  "http://www.test.com/path/to/something#fragment",
  "http://www.test.com/path/to/something?query=foo&bar=baz#fragment",
  "http://user:password@www.test.com/path/to/something#fragment",
  "http://user:password@www.test.com/path/to/something?query=foo&bar=baz#fragment",
  "http://@www.test.com/",
  "http://:@www.test.com/",
  "http://www.test.com:8080/path/to/something",
  "http://user:password@www.test.com:8080/path/to/something?query=foo#fragment",
  "http://",
  "http://www.test.com/path/to/something?#",
  "http://www.test.com/path/to/something?#fragment",
  "http:",
  "http",
  "mailto:this_is_a_urn@somedomain.com",
  ""
};

int main(int argc,char **argv)
{
  int i,r;
  char reconstbuf[2048];
  char *reconst;
  AnodeURI uri;

  for(i=0;i<22;++i) {
    printf("\"%s\":\n",testuris[i]);
    r = AnodeURI_parse(&uri,testuris[i]);
    if (r) {
      printf("  error: %d\n",r);
    } else {
      printf("  scheme: %s\n",uri.scheme);
      printf("  username: %s\n",uri.username);
      printf("  password: %s\n",uri.password);
      printf("  host: %s\n",uri.host);
      printf("  port: %d\n",uri.port);
      printf("  path: %s\n",uri.path);
      printf("  query: %s\n",uri.query);
      printf("  fragment: %s\n",uri.fragment);
    }
    reconst = AnodeURI_to_string(&uri,reconstbuf,sizeof(reconstbuf));
    printf("Reconstituted URI: %s\n",reconst ? reconst : "(null)");
    printf("\n");
  }

  return 0;
}
