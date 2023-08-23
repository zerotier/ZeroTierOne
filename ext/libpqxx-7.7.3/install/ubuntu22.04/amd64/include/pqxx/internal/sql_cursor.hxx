/** Internal wrapper for SQL cursors.  Supports higher-level cursor classes.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY.  Other headers include it for you.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_SQL_CURSOR
#define PQXX_H_SQL_CURSOR

namespace pqxx::internal
{
/// Cursor with SQL positioning semantics.
/** Thin wrapper around an SQL cursor, with SQL's ideas of positioning.
 *
 * SQL cursors have pre-increment/pre-decrement semantics, with on either end
 * of the result set a special position that does not repesent a row.  This
 * class models SQL cursors for the purpose of implementing more C++-like
 * semantics on top.
 *
 * Positions of actual rows are numbered starting at 1.  Position 0 exists but
 * does not refer to a row.  There is a similar non-row position at the end of
 * the result set.
 *
 * Don't use this at home.  You deserve better.  Use the stateles_cursor
 * instead.
 */
class PQXX_LIBEXPORT sql_cursor : public cursor_base
{
public:
  sql_cursor(
    transaction_base &t, std::string_view query, std::string_view cname,
    cursor_base::access_policy ap, cursor_base::update_policy up,
    cursor_base::ownership_policy op, bool hold);

  sql_cursor(
    transaction_base &t, std::string_view cname,
    cursor_base::ownership_policy op);

  ~sql_cursor() noexcept { close(); }

  result fetch(difference_type rows, difference_type &displacement);
  result fetch(difference_type rows)
  {
    difference_type d = 0;
    return fetch(rows, d);
  }
  difference_type move(difference_type rows, difference_type &displacement);
  difference_type move(difference_type rows)
  {
    difference_type d = 0;
    return move(rows, d);
  }

  /// Current position, or -1 for unknown
  /**
   * The starting position, just before the first row, counts as position zero.
   *
   * Position may be unknown if (and only if) this cursor was adopted, and has
   * never hit its starting position (position zero).
   */
  difference_type pos() const noexcept { return m_pos; }

  /// End position, or -1 for unknown
  /**
   * Returns the final position, just after the last row in the result set. The
   * starting position, just before the first row, counts as position zero.
   *
   * End position is unknown until it is encountered during use.
   */
  difference_type endpos() const noexcept { return m_endpos; }

  /// Return zero-row result for this cursor.
  result const &empty_result() const noexcept { return m_empty_result; }

  void close() noexcept;

private:
  difference_type adjust(difference_type hoped, difference_type actual);
  static std::string stridestring(difference_type);
  /// Initialize cached empty result.  Call only at beginning or end!
  void init_empty_result(transaction_base &);

  /// Connection in which this cursor lives.
  connection &m_home;

  /// Zero-row result from this cursor (or plain empty one if cursor is
  /// adopted)
  result m_empty_result;

  result m_cached_current_row;

  /// Is this cursor adopted (as opposed to created by this cursor object)?
  bool m_adopted;

  /// Will this cursor object destroy its SQL cursor when it dies?
  cursor_base::ownership_policy m_ownership;

  /// At starting position (-1), somewhere in the middle (0), or past end (1)
  int m_at_end;

  /// Position, or -1 for unknown
  difference_type m_pos;

  /// End position, or -1 for unknown
  difference_type m_endpos = -1;
};


PQXX_LIBEXPORT result_size_type obtain_stateless_cursor_size(sql_cursor &);
PQXX_LIBEXPORT result stateless_cursor_retrieve(
  sql_cursor &, result::difference_type size,
  result::difference_type begin_pos, result::difference_type end_pos);
} // namespace pqxx::internal
#endif
