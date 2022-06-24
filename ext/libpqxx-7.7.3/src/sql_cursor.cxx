/** Implementation of libpqxx STL-style cursor classes.
 *
 * These classes wrap SQL cursors in STL-like interfaces.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include <iterator>

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/cursor.hxx"
#include "pqxx/internal/encodings.hxx"
#include "pqxx/internal/gates/connection-sql_cursor.hxx"
#include "pqxx/internal/gates/transaction-sql_cursor.hxx"

#include "pqxx/internal/header-post.hxx"


using namespace std::literals;

namespace
{
/// Is this character a "useless trailing character" in a query?
/** A character is "useless" at the end of a query if it is either whitespace
 * or a semicolon.
 */
inline bool useless_trail(char c)
{
  return isspace(c) or c == ';';
}


/// Find end of nonempty query, stripping off any trailing semicolon.
/** When executing a normal query, a trailing semicolon is meaningless but
 * won't hurt.  That's why we can't rule out that some code may include one.
 *
 * But for cursor queries, a trailing semicolon is a problem.  The query gets
 * embedded in a larger statement, which a semicolon would break into two.
 * We'll have to remove it if present.
 *
 * A trailing semicolon may not actually be at the end.  It could be masked by
 * subsequent whitespace.  If there's also a comment though, that's the
 * caller's own lookout.  We can't guard against every possible mistake, and
 * text processing is actually remarkably sensitive to mistakes in a
 * multi-encoding world.
 *
 * If there is a trailing semicolon, this function returns its offset.  If
 * there are more than one, it returns the offset of the first one.  If there
 * is no trailing semicolon, it returns the length of the query string.
 *
 * The query must be nonempty.
 */
std::string::size_type
find_query_end(std::string_view query, pqxx::internal::encoding_group enc)
{
  auto const text{std::data(query)};
  auto const size{std::size(query)};
  std::string::size_type end;
  if (enc == pqxx::internal::encoding_group::MONOBYTE)
  {
    // This is an encoding where we can scan backwards from the end.
    for (end = std::size(query); end > 0 and useless_trail(text[end - 1]);
         --end)
      ;
  }
  else
  {
    // Complex encoding.  We only know how to iterate forwards, so start from
    // the beginning.
    end = 0;

    pqxx::internal::for_glyphs(
      enc,
      [text, &end](char const *gbegin, char const *gend) {
        if (gend - gbegin > 1 or not useless_trail(*gbegin))
          end = std::string::size_type(gend - text);
      },
      text, size);
  }

  return end;
}
} // namespace


pqxx::internal::sql_cursor::sql_cursor(
  transaction_base &t, std::string_view query, std::string_view cname,
  cursor_base::access_policy ap, cursor_base::update_policy up,
  cursor_base::ownership_policy op, bool hold) :
        cursor_base{t.conn(), cname},
        m_home{t.conn()},
        m_adopted{false},
        m_at_end{-1},
        m_pos{0}
{
  if (&t.conn() != &m_home)
    throw internal_error{"Cursor in wrong connection"};

  if (std::empty(query))
    throw usage_error{"Cursor has empty query."};
  auto const enc{enc_group(t.conn().encoding_id())};
  auto const qend{find_query_end(query, enc)};
  if (qend == 0)
    throw usage_error{"Cursor has effectively empty query."};
  query.remove_suffix(std::size(query) - qend);

  std::string const cq{internal::concat(
    "DECLARE "sv, t.quote_name(name()), " "sv,
    ((ap == cursor_base::forward_only) ? "NO "sv : ""sv), "SCROLL CURSOR "sv,
    (hold ? "WITH HOLD "sv : ""sv), "FOR "sv, query, " "sv,
    ((up == cursor_base::update) ? "FOR UPDATE "sv : "FOR READ ONLY "sv))};

  t.exec(cq);

  // Now that we're here in the starting position, keep a copy of an empty
  // result.  That may come in handy later, because we may not be able to
  // construct an empty result with all the right metadata due to the weird
  // meaning of "FETCH 0."
  init_empty_result(t);

  m_ownership = op;
}


pqxx::internal::sql_cursor::sql_cursor(
  transaction_base &t, std::string_view cname,
  cursor_base::ownership_policy op) :
        cursor_base{t.conn(), cname, false},
        m_home{t.conn()},
        m_empty_result{},
        m_adopted{true},
        m_at_end{0},
        m_pos{-1}
{
  m_adopted = true;
  m_ownership = op;
}


void pqxx::internal::sql_cursor::close() noexcept
{
  if (m_ownership == cursor_base::owned)
  {
    try
    {
      gate::connection_sql_cursor{m_home}.exec(
        internal::concat("CLOSE "sv, m_home.quote_name(name())).c_str());
    }
    catch (std::exception const &)
    {}
    m_ownership = cursor_base::loose;
  }
}


void pqxx::internal::sql_cursor::init_empty_result(transaction_base &t)
{
  if (pos() != 0)
    throw internal_error{"init_empty_result() from bad pos()."};
  m_empty_result =
    t.exec(internal::concat("FETCH 0 IN "sv, m_home.quote_name(name())));
}


/// Compute actual displacement based on requested and reported displacements.
pqxx::internal::sql_cursor::difference_type pqxx::internal::sql_cursor::adjust(
  difference_type hoped, difference_type actual)
{
  if (actual < 0)
    throw internal_error{"Negative rows in cursor movement."};
  if (hoped == 0)
    return 0;
  int const direction{((hoped < 0) ? -1 : 1)};
  bool hit_end{false};
  if (actual != labs(hoped))
  {
    if (actual > labs(hoped))
      throw internal_error{"Cursor displacement larger than requested."};

    // If we see fewer rows than requested, then we've hit an end (on either
    // side) of the result set.  Wether we make an extra step to a one-past-end
    // position or whether we're already there depends on where we were
    // previously: if our last move was in the same direction and also fell
    // short, we're already at a one-past-end row.
    if (m_at_end != direction)
      ++actual;

    // If we hit the beginning, make sure our position calculation ends up
    // at zero (even if we didn't previously know where we were!), and if we
    // hit the other end, register the fact that we now know where the end
    // of the result set is.
    if (direction > 0)
      hit_end = true;
    else if (m_pos == -1)
      m_pos = actual;
    else if (m_pos != actual)
      throw internal_error{internal::concat(
        "Moved back to beginning, but wrong position: hoped=", hoped,
        ", actual=", actual, ", m_pos=", m_pos, ", direction=", direction,
        ".")};

    m_at_end = direction;
  }
  else
  {
    m_at_end = 0;
  }

  if (m_pos >= 0)
    m_pos += direction * actual;
  if (hit_end)
  {
    if (m_endpos >= 0 and m_pos != m_endpos)
      throw internal_error{"Inconsistent cursor end positions."};
    m_endpos = m_pos;
  }
  return direction * actual;
}


pqxx::result pqxx::internal::sql_cursor::fetch(
  difference_type rows, difference_type &displacement)
{
  if (rows == 0)
  {
    displacement = 0;
    return m_empty_result;
  }
  auto const query{pqxx::internal::concat(
    "FETCH "sv, stridestring(rows), " IN "sv, m_home.quote_name(name()))};
  auto const r{gate::connection_sql_cursor{m_home}.exec(query.c_str())};
  displacement = adjust(rows, difference_type(std::size(r)));
  return r;
}


pqxx::cursor_base::difference_type pqxx::internal::sql_cursor::move(
  difference_type rows, difference_type &displacement)
{
  if (rows == 0)
  {
    displacement = 0;
    return 0;
  }

  auto const query{pqxx::internal::concat(
    "MOVE "sv, stridestring(rows), " IN "sv, m_home.quote_name(name()))};
  auto const r{gate::connection_sql_cursor{m_home}.exec(query.c_str())};
  auto d{static_cast<difference_type>(r.affected_rows())};
  displacement = adjust(rows, d);
  return d;
}


std::string pqxx::internal::sql_cursor::stridestring(difference_type n)
{
  /* Some special-casing for ALL and BACKWARD ALL here.  We used to use numeric
   * "infinities" for difference_type for this (the highest and lowest possible
   * values for "long"), but for PostgreSQL 8.0 at least, the backend appears
   * to expect a 32-bit number and fails to parse large 64-bit numbers. We
   * could change the alias to match this behaviour, but that would break
   * if/when Postgres is changed to accept 64-bit displacements.
   */
  static std::string const All{"ALL"}, BackAll{"BACKWARD ALL"};
  if (n >= cursor_base::all())
    return All;
  else if (n <= cursor_base::backward_all())
    return BackAll;
  return to_string(n);
}
