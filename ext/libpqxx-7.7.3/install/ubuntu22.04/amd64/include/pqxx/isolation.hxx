/* Definitions for transaction isolation levels, and such.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/isolation instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_ISOLATION
#define PQXX_H_ISOLATION

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include "pqxx/util.hxx"

namespace pqxx
{
/// Should a transaction be read-only, or read-write?
/** No, this is not an isolation level.  So it really doesn't belong here.
 * But it's not really worth a separate header.
 */
enum class write_policy
{
  read_only,
  read_write
};


/// Transaction isolation levels.
/** These are as defined in the SQL standard.  But there are a few notes
 * specific to PostgreSQL.
 *
 * First, postgres does not support "read uncommitted."  The lowest level you
 * can get is "read committed," which is better.  PostgreSQL is built on the
 * MVCC paradigm, which guarantees "read committed" isolation without any
 * additional performance overhead, so there was no point in providing the
 * lower level.
 *
 * Second, "repeatable read" also makes more isolation guarantees than the
 * standard requires.  According to the standard, this level prevents "dirty
 * reads" and "nonrepeatable reads," but not "phantom reads."  In postgres,
 * it actually prevents all three.
 *
 * Third, "serializable" is only properly supported starting at postgres 9.1.
 * If you request "serializable" isolation on an older backend, you will get
 * the same isolation as in "repeatable read."  It's better than the
 * "repeatable read" defined in the SQL standard, but not a complete
 * implementation of the standard's "serializable" isolation level.
 *
 * In general, a lower isolation level will allow more surprising interactions
 * between ongoing transactions, but improve performance.  A higher level
 * gives you more protection from subtle concurrency bugs, but sometimes it
 * may not be possible to complete your transaction without avoiding paradoxes
 * in the data.  In that case a transaction may fail, and the application will
 * have to re-do the whole thing based on the latest state of the database.
 * (If you want to retry your code in that situation, have a look at the
 * transactor framework.)
 *
 * Study the levels and design your application with the right level in mind.
 */
enum isolation_level
{
  // PostgreSQL only has the better isolation levels.
  // read_uncommitted,

  read_committed,
  repeatable_read,
  serializable,
};
} // namespace pqxx
#endif
