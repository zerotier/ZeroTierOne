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

#include "anode.h"

struct AnodeErrDesc
{
  int code;
  const char *desc;
};

#define TOTAL_ERRORS 12
static const struct AnodeErrDesc ANODE_ERRORS[TOTAL_ERRORS] = {
  { ANODE_ERR_NONE, "No error (success)" },
  { ANODE_ERR_INVALID_ARGUMENT, "Invalid argument" },
  { ANODE_ERR_OUT_OF_MEMORY, "Out of memory" },
  { ANODE_ERR_INVALID_URI, "Invalid URI" },
  { ANODE_ERR_BUFFER_TOO_SMALL, "Supplied buffer too small" },
  { ANODE_ERR_ADDRESS_INVALID, "Address invalid" },
  { ANODE_ERR_ADDRESS_TYPE_NOT_SUPPORTED, "Address type not supported"},
  { ANODE_ERR_CONNECTION_CLOSED, "Connection closed"},
  { ANODE_ERR_CONNECT_FAILED, "Connect failed"},
  { ANODE_ERR_UNABLE_TO_BIND, "Unable to bind to address"},
  { ANODE_ERR_TOO_MANY_OPEN_SOCKETS, "Too many open sockets"},
  { ANODE_ERR_DNS_NAME_NOT_FOUND_OR_TIMED_OUT, "DNS name not found or timed out"}
};

extern const char *Anode_strerror(int err)
{
  int i;
  int negerr = -err;

  for(i=0;i<TOTAL_ERRORS;++i) {
    if ((ANODE_ERRORS[i].code == err)||(ANODE_ERRORS[i].code == negerr))
      return ANODE_ERRORS[i].desc;
  }

  return "Unknown error";
}
