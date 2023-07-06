/* Definition of the iterator/container-style cursor classes.
 *
 * C++-style wrappers for SQL cursors.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/cursor instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_CURSOR
#define PQXX_H_CURSOR

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <limits>
#include <stdexcept>

#include "pqxx/result.hxx"
#include "pqxx/transaction_base.hxx"


namespace pqxx
{
/// Common definitions for cursor types
/** In C++ terms, fetches are always done in pre-increment or pre-decrement
 * fashion--i.e. the result does not include the row the cursor is on at the
 * beginning of the fetch, and the cursor ends up being positioned on the last
 * row in the result.
 *
 * There are singular positions akin to `end()` at both the beginning and the
 * end of the cursor's range of movement, although these fit in so naturally
 * with the semantics that one rarely notices them.  The cursor begins at the
 * first of these, but any fetch in the forward direction will move the cursor
 * off this position and onto the first row before returning anything.
 */
class PQXX_LIBEXPORT cursor_base
{
public:
  using size_type = result_size_type;
  using difference_type = result_difference_type;

  /// Cursor access-pattern policy
  /** Allowing a cursor to move forward only can result in better performance,
   * so use this access policy whenever possible.
   */
  enum access_policy
  {
    /// Cursor can move forward only
    forward_only,
    /// Cursor can move back and forth
    random_access
  };

  /// Cursor update policy
  /**
   * @warning Not all PostgreSQL versions support updatable cursors.
   */
  enum update_policy
  {
    /// Cursor can be used to read data but not to write
    read_only,
    /// Cursor can be used to update data as well as read it
    update
  };

  /// Cursor destruction policy
  /** The normal thing to do is to make a cursor object the owner of the SQL
   * cursor it represents.  There may be cases, however, where a cursor needs
   * to persist beyond the end of the current transaction (and thus also beyond
   * the lifetime of the cursor object that created it!), where it can be
   * "adopted" into a new cursor object.  See the basic_cursor documentation
   * for an explanation of cursor adoption.
   *
   * If a cursor is created with "loose" ownership policy, the object
   * representing the underlying SQL cursor will not take the latter with it
   * when its own lifetime ends, nor will its originating transaction.
   *
   * @warning Use this feature with care and moderation.  Only one cursor
   * object should be responsible for any one underlying SQL cursor at any
   * given time.
   */
  enum ownership_policy
  {
    /// Destroy SQL cursor when cursor object is closed at end of transaction
    owned,
    /// Leave SQL cursor in existence after close of object and transaction
    loose
  };

  cursor_base() = delete;
  cursor_base(cursor_base const &) = delete;
  cursor_base &operator=(cursor_base const &) = delete;

  /**
   * @name Special movement distances.
   */
  //@{

  // TODO: Make constexpr inline (but breaks ABI).
  /// Special value: read until end.
  /** @return Maximum value for result::difference_type, so the cursor will
   * attempt to read the largest possible result set.
   */
  [[nodiscard]] static difference_type all() noexcept;

  /// Special value: read one row only.
  /** @return Unsurprisingly, 1.
   */
  [[nodiscard]] static constexpr difference_type next() noexcept { return 1; }

  /// Special value: read backwards, one row only.
  /** @return Unsurprisingly, -1.
   */
  [[nodiscard]] static constexpr difference_type prior() noexcept
  {
    return -1;
  }

  // TODO: Make constexpr inline (but breaks ABI).
  /// Special value: read backwards from current position back to origin.
  /** @return Minimum value for result::difference_type.
   */
  [[nodiscard]] static difference_type backward_all() noexcept;

  //@}

  /// Name of underlying SQL cursor
  /**
   * @returns Name of SQL cursor, which may differ from original given name.
   * @warning Don't use this to access the SQL cursor directly without going
   * through the provided wrapper classes!
   */
  [[nodiscard]] constexpr std::string const &name() const noexcept
  {
    return m_name;
  }

protected:
  cursor_base(connection &, std::string_view Name, bool embellish_name = true);

  std::string const m_name;
};
} // namespace pqxx


#include <pqxx/internal/sql_cursor.hxx>


namespace pqxx
{
/// "Stateless cursor" class: easy API for retrieving parts of result sets
/** This is a front-end for SQL cursors, but with a more C++-like API.
 *
 * Actually, stateless_cursor feels entirely different from SQL cursors.  You
 * don't keep track of positions, fetches, and moves; you just say which rows
 * you want.  See the retrieve() member function.
 */
template<cursor_base::update_policy up, cursor_base::ownership_policy op>
class stateless_cursor
{
public:
  using size_type = result_size_type;
  using difference_type = result_difference_type;

  /// Create cursor.
  /**
   * @param tx The transaction within which you want to create the cursor.
   * @param query The SQL query whose results the cursor should traverse.
   * @param cname A hint for the cursor's name.  The actual SQL cursor's name
   *     will be based on this (though not necessarily identical).
   * @param hold Create a `WITH HOLD` cursor?  Such cursors stay alive after
   *     the transaction has ended, so you can continue to use it.
   */
  stateless_cursor(
    transaction_base &tx, std::string_view query, std::string_view cname,
    bool hold) :
          m_cur{tx, query, cname, cursor_base::random_access, up, op, hold}
  {}

  /// Adopt an existing scrolling SQL cursor.
  /** This lets you define a cursor yourself, and then wrap it in a
   * libpqxx-managed `stateless_cursor` object.
   *
   * @param tx The transaction within which you want to manage the cursor.
   * @param adopted_cursor Your cursor's SQL name.
   */
  stateless_cursor(transaction_base &tx, std::string_view adopted_cursor) :
          m_cur{tx, adopted_cursor, op}
  {
    // Put cursor in known position
    m_cur.move(cursor_base::backward_all());
  }

  /// Close this cursor.
  /** The destructor will do this for you automatically.
   *
   * Closing a cursor is idempotent.  Closing a cursor that's already closed
   * does nothing.
   */
  void close() noexcept { m_cur.close(); }

  /// Number of rows in cursor's result set
  /** @note This function is not const; it may need to scroll to find the size
   * of the result set.
   */
  [[nodiscard]] size_type size()
  {
    return internal::obtain_stateless_cursor_size(m_cur);
  }

  /// Retrieve rows from begin_pos (inclusive) to end_pos (exclusive)
  /** Rows are numbered starting from 0 to size()-1.
   *
   * @param begin_pos First row to retrieve.  May be one row beyond the end of
   * the result set, to avoid errors for empty result sets.  Otherwise, must be
   * a valid row number in the result set.
   * @param end_pos Row up to which to fetch.  Rows are returned ordered from
   * begin_pos to end_pos, i.e. in ascending order if begin_pos < end_pos but
   * in descending order if begin_pos > end_pos.  The end_pos may be
   * arbitrarily inside or outside the result set; only existing rows are
   * included in the result.
   */
  result retrieve(difference_type begin_pos, difference_type end_pos)
  {
    return internal::stateless_cursor_retrieve(
      m_cur, result::difference_type(size()), begin_pos, end_pos);
  }

  /// Return this cursor's name.
  [[nodiscard]] constexpr std::string const &name() const noexcept
  {
    return m_cur.name();
  }

private:
  internal::sql_cursor m_cur;
};


class icursor_iterator;
} // namespace pqxx


namespace pqxx::internal::gate
{
class icursor_iterator_icursorstream;
class icursorstream_icursor_iterator;
} // namespace pqxx::internal::gate


namespace pqxx
{
/// Simple read-only cursor represented as a stream of results
/** SQL cursors can be tricky, especially in C++ since the two languages seem
 * to have been designed on different planets.  An SQL cursor has two singular
 * positions akin to `end()` on either side of the underlying result set.
 *
 * These cultural differences are hidden from view somewhat by libpqxx, which
 * tries to make SQL cursors behave more like familiar C++ entities such as
 * iterators, sequences, streams, and containers.
 *
 * Data is fetched from the cursor as a sequence of result objects.  Each of
 * these will contain the number of rows defined as the stream's stride, except
 * of course the last block of data which may contain fewer rows.
 *
 * This class can create or adopt cursors that live outside any backend
 * transaction, which your backend version may not support.
 */
class PQXX_LIBEXPORT icursorstream
{
public:
  using size_type = cursor_base::size_type;
  using difference_type = cursor_base::difference_type;

  /// Set up a read-only, forward-only cursor.
  /** Roughly equivalent to a C++ Standard Library istream, this cursor type
   * supports only two operations: reading a block of rows while moving
   * forward, and moving forward without reading any data.
   *
   * @param context Transaction context in which this cursor will be active.
   * @param query SQL query whose results this cursor shall iterate.
   * @param basename Suggested name for the SQL cursor; the library will append
   * a unique code to ensure its uniqueness.
   * @param sstride Number of rows to fetch per read operation; must be a
   * positive number.
   */
  icursorstream(
    transaction_base &context, std::string_view query,
    std::string_view basename, difference_type sstride = 1);

  /// Adopt existing SQL cursor.  Use with care.
  /** Forms a cursor stream around an existing SQL cursor, as returned by e.g.
   * a server-side function.  The SQL cursor will be cleaned up by the stream's
   * destructor as if it had been created by the stream; cleaning it up by hand
   * or adopting the same cursor twice is an error.
   *
   * Passing the name of the cursor as a string is not allowed, both to avoid
   * confusion with the other constructor and to discourage unnecessary use of
   * adopted cursors.
   *
   * @warning It is technically possible to adopt a "WITH HOLD" cursor, i.e. a
   * cursor that stays alive outside its creating transaction.  However, any
   * cursor stream (including the underlying SQL cursor, naturally) must be
   * destroyed before its transaction context object is destroyed.  Therefore
   * the only way to use SQL's WITH HOLD feature is to adopt the cursor, but
   * defer doing so until after entering the transaction context that will
   * eventually destroy it.
   *
   * @param context Transaction context in which this cursor will be active.
   * @param cname Result field containing the name of the SQL cursor to adopt.
   * @param sstride Number of rows to fetch per read operation; must be a
   * positive number.
   * @param op Ownership policy.  Determines whether the cursor underlying this
   * stream will be destroyed when the stream is closed.
   */
  icursorstream(
    transaction_base &context, field const &cname, difference_type sstride = 1,
    cursor_base::ownership_policy op = cursor_base::owned);

  /// Return `true` if this stream may still return more data.
  constexpr operator bool() const &noexcept { return not m_done; }

  /// Read new value into given result object; same as operator `>>`.
  /** The result set may continue any number of rows from zero to the chosen
   * stride, inclusive.  An empty result will only be returned if there are no
   * more rows to retrieve.
   *
   * @param res Write the retrieved data into this result object.
   * @return Reference to this very stream, to facilitate "chained" invocations
   * ("C.get(r1).get(r2);")
   */
  icursorstream &get(result &res)
  {
    res = fetchblock();
    return *this;
  }
  /// Read new value into given result object; same as `get(result&)`.
  /** The result set may continue any number of rows from zero to the chosen
   * stride, inclusive.  An empty result will only be returned if there are no
   * more rows to retrieve.
   *
   * @param res Write the retrieved data into this result object.
   * @return Reference to this very stream, to facilitate "chained" invocations
   * ("C >> r1 >> r2;")
   */
  icursorstream &operator>>(result &res) { return get(res); }

  /// Move given number of rows forward without reading data.
  /** Ignores any stride that you may have set.  It moves by a given number of
   * rows, not a number of strides.
   *
   * @return Reference to this stream itself, to facilitate "chained"
   * invocations.
   */
  icursorstream &ignore(std::streamsize n = 1) &;

  /// Change stride, i.e. the number of rows to fetch per read operation.
  /**
   * @param stride Must be a positive number.
   */
  void set_stride(difference_type stride) &;
  [[nodiscard]] constexpr difference_type stride() const noexcept
  {
    return m_stride;
  }

private:
  result fetchblock();

  friend class internal::gate::icursorstream_icursor_iterator;
  size_type forward(size_type n = 1);
  void insert_iterator(icursor_iterator *) noexcept;
  void remove_iterator(icursor_iterator *) const noexcept;

  void service_iterators(difference_type);

  internal::sql_cursor m_cur;

  difference_type m_stride;
  difference_type m_realpos, m_reqpos;

  mutable icursor_iterator *m_iterators;

  bool m_done;
};


/// Approximate istream_iterator for icursorstream.
/** Intended as an implementation of an input_iterator (as defined by the C++
 * Standard Library), this class supports only two basic operations: reading
 * the current element, and moving forward.  In addition to the minimal
 * guarantees for istream_iterators, this class supports multiple successive
 * reads of the same position (the current result set is cached in the
 * iterator) even after copying and even after new data have been read from the
 * stream.  This appears to be a requirement for input_iterators.  Comparisons
 * are also supported in the general case.
 *
 * The iterator does not care about its own position, however.  Moving an
 * iterator forward moves the underlying stream forward and reads the data from
 * the new stream position, regardless of the iterator's old position in the
 * stream.
 *
 * The stream's stride defines the granularity for all iterator movement or
 * access operations, i.e. "ici += 1" advances the stream by one stride's worth
 * of rows, and "*ici++" reads one stride's worth of rows from the stream.
 *
 * @warning Do not read from the underlying stream or its cursor, move its read
 * position, or change its stride, between the time the first icursor_iterator
 * on it is created and the time its last icursor_iterator is destroyed.
 *
 * @warning Manipulating these iterators within the context of a single cursor
 * stream is <em>not thread-safe</em>.  Creating a new iterator, copying one,
 * or destroying one affects the stream as a whole.
 */
class PQXX_LIBEXPORT icursor_iterator
{
public:
  using iterator_category = std::input_iterator_tag;
  using value_type = result;
  using pointer = result const *;
  using reference = result const &;
  using istream_type = icursorstream;
  using size_type = istream_type::size_type;
  using difference_type = istream_type::difference_type;

  icursor_iterator() noexcept;
  explicit icursor_iterator(istream_type &) noexcept;
  icursor_iterator(icursor_iterator const &) noexcept;
  ~icursor_iterator() noexcept;

  result const &operator*() const
  {
    refresh();
    return m_here;
  }
  result const *operator->() const
  {
    refresh();
    return &m_here;
  }
  icursor_iterator &operator++();
  icursor_iterator operator++(int);
  icursor_iterator &operator+=(difference_type);
  icursor_iterator &operator=(icursor_iterator const &) noexcept;

  [[nodiscard]] bool operator==(icursor_iterator const &rhs) const;
  [[nodiscard]] bool operator!=(icursor_iterator const &rhs) const noexcept
  {
    return not operator==(rhs);
  }
  [[nodiscard]] bool operator<(icursor_iterator const &rhs) const;
  [[nodiscard]] bool operator>(icursor_iterator const &rhs) const
  {
    return rhs < *this;
  }
  [[nodiscard]] bool operator<=(icursor_iterator const &rhs) const
  {
    return not(*this > rhs);
  }
  [[nodiscard]] bool operator>=(icursor_iterator const &rhs) const
  {
    return not(*this < rhs);
  }

private:
  void refresh() const;

  friend class internal::gate::icursor_iterator_icursorstream;
  difference_type pos() const noexcept { return m_pos; }
  void fill(result const &);

  icursorstream *m_stream{nullptr};
  result m_here;
  difference_type m_pos;
  icursor_iterator *m_prev{nullptr}, *m_next{nullptr};
};
} // namespace pqxx
#endif
