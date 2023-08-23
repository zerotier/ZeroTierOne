/* Definition of the pqxx::stream_from class.
 *
 * pqxx::stream_from enables optimized batch reads from a database table.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/stream_from instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_STREAM_FROM
#define PQXX_H_STREAM_FROM

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <cassert>
#include <functional>
#include <variant>

#include "pqxx/connection.hxx"
#include "pqxx/except.hxx"
#include "pqxx/internal/concat.hxx"
#include "pqxx/internal/encoding_group.hxx"
#include "pqxx/internal/stream_iterator.hxx"
#include "pqxx/separated_list.hxx"
#include "pqxx/transaction_focus.hxx"


namespace pqxx
{
class transaction_base;


/// Pass this to a `stream_from` constructor to stream table contents.
/** @deprecated Use @ref stream_from::table() instead.
 */
constexpr from_table_t from_table;
/// Pass this to a `stream_from` constructor to stream query results.
/** @deprecated Use stream_from::query() instead.
 */
constexpr from_query_t from_query;


/// Stream data from the database.
/** For larger data sets, retrieving data this way is likely to be faster than
 * executing a query and then iterating and converting the rows fields.  You
 * will also be able to start processing before all of the data has come in.
 *
 * There are also downsides.  Not all kinds of query will work in a stream.
 * But straightforward `SELECT` and `UPDATE ... RETURNING` queries should work.
 * This function makes use of @ref pqxx::stream_from, which in turn uses
 * PostgreSQL's `COPY` command, so see the documentation for those to get the
 * full details.
 *
 * There are other downsides.  If there stream encounters an error, it may
 * leave the entire connection in an unusable state, so you'll have to give the
 * whole thing up.  Finally, opening a stream puts the connection in a special
 * state, so you won't be able to do many other things with the connection or
 * the transaction while the stream is open.
 *
 * There are two ways of starting a stream: you stream either all rows in a
 * table (using one of the factories, `table()` or `raw_table()`), or the
 * results of a query (using the `query()` factory).
 *
 * Usually you'll want the `stream` convenience wrapper in
 * @ref transaction_base, * so you don't need to deal with this class directly.
 *
 * @warning While a stream is active, you cannot execute queries, open a
 * pipeline, etc. on the same transaction.  A transaction can have at most one
 * object of a type derived from @ref pqxx::transaction_focus active on it at a
 * time.
 */
class PQXX_LIBEXPORT stream_from : transaction_focus
{
public:
  using raw_line =
    std::pair<std::unique_ptr<char, std::function<void(char *)>>, std::size_t>;

  /// Factory: Execute query, and stream the results.
  /** The query can be a SELECT query or a VALUES query; or it can be an
   * UPDATE, INSERT, or DELETE with a RETURNING clause.
   *
   * The query is executed as part of a COPY statement, so there are additional
   * restrictions on what kind of query you can use here.  See the PostgreSQL
   * documentation for the COPY command:
   *
   *     https://www.postgresql.org/docs/current/sql-copy.html
   */
  static stream_from query(transaction_base &tx, std::string_view q)
  {
#include "pqxx/internal/ignore-deprecated-pre.hxx"
    return {tx, from_query, q};
#include "pqxx/internal/ignore-deprecated-post.hxx"
  }

  /**
   * @name Streaming data from tables
   *
   * You can use `stream_from` to read a table's contents.  This is a quick
   * and easy way to read a table, but it comes with limitations.  It cannot
   * stream from a view, only from a table.  It does not support conditions.
   * And there are no guarantees about ordering.  If you need any of those
   * things, consider streaming from a query instead.
   */
  //@{

  /// Factory: Stream data from a pre-quoted table and columns.
  /** Use this factory if you need to create multiple streams using the same
   * table path and/or columns list, and you want to save a bit of work on
   * composing the internal SQL statement for starting the stream.  It lets you
   * compose the string representations for the table path and the columns
   * list, so you can compute these once and then re-use them later.
   *
   * @param tx The transaction within which the stream will operate.
   * @param path Name or path for the table upon which the stream will
   *     operate.  If any part of the table path may contain special
   *     characters or be case-sensitive, quote the path using
   *     pqxx::connection::quote_table().
   * @param columns Columns which the stream will read.  They should be
   *     comma-separated and, if needed, quoted.  You can produce the string
   *     using pqxx::connection::quote_columns().  If you omit this argument,
   *     the stream will read all columns in the table, in schema order.
   */
  static stream_from raw_table(
    transaction_base &tx, std::string_view path,
    std::string_view columns = ""sv);

  /// Factory: Stream data from a given table.
  /** This is the convenient way to stream from a table.
   */
  static stream_from table(
    transaction_base &tx, table_path path,
    std::initializer_list<std::string_view> columns = {});
  //@}

  /// Execute query, and stream over the results.
  /** @deprecated Use factory function @ref query instead.
   */
  [[deprecated("Use query() factory instead.")]] stream_from(
    transaction_base &, from_query_t, std::string_view query);

  /// Stream all rows in table, all columns.
  /** @deprecated Use factories @ref table or @ref raw_table instead.
   */
  [[deprecated("Use table() or raw_table() factory instead.")]] stream_from(
    transaction_base &, from_table_t, std::string_view table);

  /// Stream given columns from all rows in table.
  /** @deprecated Use factories @ref table or @ref raw_table instead.
   */
  template<typename Iter>
  [[deprecated("Use table() or raw_table() factory instead.")]] stream_from(
    transaction_base &, from_table_t, std::string_view table,
    Iter columns_begin, Iter columns_end);

  /// Stream given columns from all rows in table.
  /** @deprecated Use factory function @ref query instead.
   */
  template<typename Columns>
  [[deprecated("Use table() or raw_table() factory instead.")]] stream_from(
    transaction_base &tx, from_table_t, std::string_view table,
    Columns const &columns);

#include "pqxx/internal/ignore-deprecated-pre.hxx"
  /// @deprecated Use factories @ref table or @ref raw_table instead.
  [[deprecated("Use the from_table_t overload instead.")]] stream_from(
    transaction_base &tx, std::string_view table) :
          stream_from{tx, from_table, table}
  {}
#include "pqxx/internal/ignore-deprecated-post.hxx"

  /// @deprecated Use factories @ref table or @ref raw_table instead.
  template<typename Columns>
  [[deprecated("Use the from_table_t overload instead.")]] stream_from(
    transaction_base &tx, std::string_view table, Columns const &columns) :
          stream_from{tx, from_table, table, columns}
  {}

  /// @deprecated Use factories @ref table or @ref raw_table instead.
  template<typename Iter>
  [[deprecated("Use the from_table_t overload instead.")]] stream_from(
    transaction_base &, std::string_view table, Iter columns_begin,
    Iter columns_end);

  ~stream_from() noexcept;

  /// May this stream still produce more data?
  [[nodiscard]] constexpr operator bool() const noexcept
  {
    return not m_finished;
  }
  /// Has this stream produced all the data it is going to produce?
  [[nodiscard]] constexpr bool operator!() const noexcept
  {
    return m_finished;
  }

  /// Finish this stream.  Call this before continuing to use the connection.
  /** Consumes all remaining lines, and closes the stream.
   *
   * This may take a while if you're abandoning the stream before it's done, so
   * skip it in error scenarios where you're not planning to use the connection
   * again afterwards.
   */
  void complete();

  /// Read one row into a tuple.
  /** Converts the row's fields into the fields making up the tuple.
   *
   * For a column which can contain nulls, be sure to give the corresponding
   * tuple field a type which can be null.  For example, to read a field as
   * `int` when it may contain nulls, read it as `std::optional<int>`.
   * Using `std::shared_ptr` or `std::unique_ptr` will also work.
   */
  template<typename Tuple> stream_from &operator>>(Tuple &);

  /// Doing this with a `std::variant` is going to be horrifically borked.
  template<typename... Vs>
  stream_from &operator>>(std::variant<Vs...> &) = delete;

  /// Iterate over this stream.  Supports range-based "for" loops.
  /** Produces an input iterator over the stream.
   *
   * Do not call this yourself.  Use it like "for (auto data : stream.iter())".
   */
  template<typename... TYPE> [[nodiscard]] auto iter() &
  {
    return pqxx::internal::stream_input_iteration<TYPE...>{*this};
  }

  /// Read a row.  Return fields as views, valid until you read the next row.
  /** Returns `nullptr` when there are no more rows to read.  Do not attempt
   * to read any further rows after that.
   *
   * Do not access the vector, or the storage referenced by the views, after
   * closing or completing the stream, or after attempting to read a next row.
   *
   * A @ref pqxx::zview is like a `std::string_view`, but with the added
   * guarantee that if its data pointer is non-null, the string is followed by
   * a terminating zero (which falls just outside the view itself).
   *
   * If any of the views' data pointer is null, that means that the
   * corresponding SQL field is null.
   *
   * @warning The return type may change in the future, to support C++20
   * coroutine-based usage.
   */
  std::vector<zview> const *read_row() &;

  /// Read a raw line of text from the COPY command.
  /** @warning Do not use this unless you really know what you're doing. */
  raw_line get_raw_line();

private:
  // TODO: Clean up this signature once we cull the deprecated constructors.
  /// @deprecated
  stream_from(
    transaction_base &tx, std::string_view table, std::string_view columns,
    from_table_t);

  // TODO: Clean up this signature once we cull the deprecated constructors.
  /// @deprecated
  stream_from(
    transaction_base &, std::string_view unquoted_table,
    std::string_view columns, from_table_t, int);

  template<typename Tuple, std::size_t... indexes>
  void extract_fields(Tuple &t, std::index_sequence<indexes...>) const
  {
    (extract_value<Tuple, indexes>(t), ...);
  }

  pqxx::internal::glyph_scanner_func *m_glyph_scanner;

  /// Current row's fields' text, combined into one reusable string.
  std::string m_row;

  /// The current row's fields.
  std::vector<zview> m_fields;

  bool m_finished = false;

  void close();

  template<typename Tuple, std::size_t index>
  void extract_value(Tuple &) const;

  /// Read a line of COPY data, write `m_row` and `m_fields`.
  void parse_line();
};


template<typename Columns>
inline stream_from::stream_from(
  transaction_base &tx, from_table_t, std::string_view table_name,
  Columns const &columns) :
        stream_from{
          tx, from_table, table_name, std::begin(columns), std::end(columns)}
{}


template<typename Iter>
inline stream_from::stream_from(
  transaction_base &tx, from_table_t, std::string_view table,
  Iter columns_begin, Iter columns_end) :
        stream_from{
          tx, table, separated_list(",", columns_begin, columns_end),
          from_table, 1}
{}


template<typename Tuple> inline stream_from &stream_from::operator>>(Tuple &t)
{
  if (m_finished)
    return *this;
  static constexpr auto tup_size{std::tuple_size_v<Tuple>};
  m_fields.reserve(tup_size);
  parse_line();
  if (m_finished)
    return *this;

  if (std::size(m_fields) != tup_size)
    throw usage_error{internal::concat(
      "Tried to extract ", tup_size, " field(s) from a stream of ",
      std::size(m_fields), ".")};

  extract_fields(t, std::make_index_sequence<tup_size>{});
  return *this;
}


template<typename Tuple, std::size_t index>
inline void stream_from::extract_value(Tuple &t) const
{
  using field_type = strip_t<decltype(std::get<index>(t))>;
  using nullity = nullness<field_type>;
  assert(index < std::size(m_fields));
  if constexpr (nullity::always_null)
  {
    if (std::data(m_fields[index]) != nullptr)
      throw conversion_error{"Streaming non-null value into null field."};
  }
  else if (std::data(m_fields[index]) == nullptr)
  {
    if constexpr (nullity::has_null)
      std::get<index>(t) = nullity::null();
    else
      internal::throw_null_conversion(type_name<field_type>);
  }
  else
  {
    // Don't ever try to convert a non-null value to nullptr_t!
    std::get<index>(t) = from_string<field_type>(m_fields[index]);
  }
}
} // namespace pqxx
#endif
