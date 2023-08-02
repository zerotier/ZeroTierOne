/** Minimal forward declarations of libpq types needed in libpqxx headers.
 *
 * DO NOT INCLUDE THIS FILE when building client programs.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
extern "C"
{
  struct pg_conn;
  struct pg_result;
  struct pgNotify;
}

/// Forward declarations of libpq types as needed in libpqxx headers.
namespace pqxx::internal::pq
{
using PGconn = pg_conn;
using PGresult = pg_result;
using PGnotify = pgNotify;
using PQnoticeProcessor = void (*)(void *, char const *);
} // namespace pqxx::internal::pq

namespace pqxx
{
/// PostgreSQL database row identifier.
using oid = unsigned int;
} // namespace pqxx
