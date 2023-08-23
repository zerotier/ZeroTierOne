/* Definition of the pqxx::stream_to class.
 *
 * pqxx::stream_to enables optimized batch updates to a database table.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/stream_to.hxx instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_STREAM_TO
#define PQXX_H_STREAM_TO

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include "pqxx/separated_list.hxx"
#include "pqxx/transaction_base.hxx"


namespace pqxx
{
/// Efficiently write data directly to a database table.
/** If you wish to insert rows of data into a table, you can compose INSERT
 * statements and execute them.  But it's slow and tedious, and you need to
 * worry about quoting and escaping the data.
 *
 * If you're just inserting a single row, it probably won't matter much.  You
 * can use prepared or parameterised statements to take care of the escaping
 * for you.  But if you're inserting large numbers of rows you will want
 * something better.
 *
 * Inserting rows one by one using INSERT statements involves a lot of
 * pointless overhead, especially when you are working with a remote database
 * server over the network.  You may end up sending each row over the network
 * as a separate query, and waiting for a reply.  Do it "in bulk" using
 * `stream_to`, and you may find that it goes many times faster.  Sometimes
 * you gain orders of magnitude in speed.
 *
 * Here's how it works: you create a `stream_to` stream to start writing to
 * your table.  You will probably want to specify the columns.  Then, you
 * feed your data into the stream one row at a time.  And finally, you call the
 * stream's @ref complete function to tell it to finalise the operation, wait
 * for completion, and check for errors.
 *
 * (You _must_ complete the stream before committing or aborting the
 * transaction.  The connection is in a special state while the stream is
 * active, where it can't process commands, and can't commit or abort a
 * transaction.)
 *
 * So how do you feed a row of data into the stream?  There's several ways, but
 * the preferred one is to call its @ref write_values.  Pass the field values
 * as arguments.  Doesn't matter what type they are, as long as libpqxx knows
 * how to convert them to PostgreSQL's text format: `int`, `std::string` or
 * `std:string_view`, `float` and `double`, `bool`...  lots of basic types
 * are supported.  If some of the values are null, feel free to use
 * `std::optional`, `std::shared_ptr`, or `std::unique_ptr`.
 *
 * The arguments' types don't even have to match the fields' SQL types.  If you
 * want to insert an `int` into a `DECIMAL` column, that's your choice -- it
 * will produce a `DECIMAL` value which happens to be integral.  Insert a
 * `float` into a `VARCHAR` column?  That's fine, you'll get a string whose
 * contents happen to read like a number.  And so on.  You can even insert
 * different types of value in the same column on different rows.  If you have
 * a code path where a particular field is always null, just insert `nullptr`.
 *
 * There is another way to insert rows: the `<<` ("shift-left") operator.
 * It's not as fast and it doesn't support variable arguments: each row must be
 * either a `std::tuple` or something iterable, such as a `std::vector`, or
 * anything else with a `begin()` and `end()`.
 *
 * @warning While a stream is active, you cannot execute queries, open a
 * pipeline, etc. on the same transaction.  A transaction can have at most one
 * object of a type derived from @ref pqxx::transaction_focus active on it at a
 * time.
 */
class PQXX_LIBEXPORT stream_to : transaction_focus
{
public:
  /// Stream data to a pre-quoted table and columns.
  /** This factory can be useful when it's not convenient to provide the
   * columns list in the form of a `std::initializer_list`, or when the list
   * of columns is simply not known at compile time.
   *
   * Also use this if you need to create multiple streams using the same table
   * path and/or columns list, and you want to save a bit of work on composing
   * the internal SQL statement for starting the stream.  It lets you compose
   * the string representations for the table path and the columns list, so you
   * can compute these once and then re-use them later.
   *
   * @param tx The transaction within which the stream will operate.
   * @param path Name or path for the table upon which the stream will
   *     operate.  If any part of the table path may contain special
   *     characters or be case-sensitive, quote the path using
   *     pqxx::connection::quote_table().
   * @param columns Columns to which the stream will write.  They should be
   *     comma-separated and, if needed, quoted.  You can produce the string
   *     using pqxx::connection::quote_columns().  If you omit this argument,
   *     the stream will write all columns in the table, in schema order.
   */
  static stream_to raw_table(
    transaction_base &tx, std::string_view path, std::string_view columns = "")
  {
    return {tx, path, columns};
  }

  /// Create a `stream_to` writing to a named table and columns.
  /** Use this to stream data to a table, where the list of columns is known at
   * compile time.
   *
   * @param tx The transaction within which the stream will operate.
   * @param path A @ref table_path designating the target table.
   * @param columns Optionally, the columns to which the stream should write.
   *     If you do not pass this, the stream will write to all columns in the
   *     table, in schema order.
   */
  static stream_to table(
    transaction_base &tx, table_path path,
    std::initializer_list<std::string_view> columns = {})
  {
    auto const &conn{tx.conn()};
    return raw_table(tx, conn.quote_table(path), conn.quote_columns(columns));
  }

#if defined(PQXX_HAVE_CONCEPTS)
  /// Create a `stream_to` writing to a named table and columns.
  /** Use this version to stream data to a table, when the list of columns is
   * not known at compile time.
   *
   * @param tx The transaction within which the stream will operate.
   * @param path A @ref table_path designating the target table.
   * @param columns The columns to which the stream should write.
   */
  template<PQXX_CHAR_STRINGS_ARG COLUMNS>
  static stream_to
  table(transaction_base &tx, table_path path, COLUMNS const &columns)
  {
    auto const &conn{tx.conn()};
    return stream_to::raw_table(
      tx, conn.quote_table(path), tx.conn().quote_columns(columns));
  }

  /// Create a `stream_to` writing to a named table and columns.
  /** Use this version to stream data to a table, when the list of columns is
   * not known at compile time.
   *
   * @param tx The transaction within which the stream will operate.
   * @param path A @ref table_path designating the target table.
   * @param columns The columns to which the stream should write.
   */
  template<PQXX_CHAR_STRINGS_ARG COLUMNS>
  static stream_to
  table(transaction_base &tx, std::string_view path, COLUMNS const &columns)
  {
    return stream_to::raw_table(tx, path, tx.conn().quote_columns(columns));
  }
#endif // PQXX_HAVE_CONCEPTS

  /// Create a stream, without specifying columns.
  /** @deprecated Use @ref table or @ref raw_table as a factory.
   *
   * Fields will be inserted in whatever order the columns have in the
   * database.
   *
   * You'll probably want to specify the columns, so that the mapping between
   * your data fields and the table is explicit in your code, and not hidden
   * in an "implicit contract" between your code and your schema.
   */
  [[deprecated("Use table() or raw_table() factory.")]] stream_to(
    transaction_base &tx, std::string_view table_name) :
          stream_to{tx, table_name, ""sv}
  {}

  /// Create a stream, specifying column names as a container of strings.
  /** @deprecated Use @ref table or @ref raw_table as a factory.
   */
  template<typename Columns>
  [[deprecated("Use table() or raw_table() factory.")]] stream_to(
    transaction_base &, std::string_view table_name, Columns const &columns);

  /// Create a stream, specifying column names as a sequence of strings.
  /** @deprecated Use @ref table or @ref raw_table as a factory.
   */
  template<typename Iter>
  [[deprecated("Use table() or raw_table() factory.")]] stream_to(
    transaction_base &, std::string_view table_name, Iter columns_begin,
    Iter columns_end);

  ~stream_to() noexcept;

  /// Does this stream still need to @ref complete()?
  [[nodiscard]] constexpr operator bool() const noexcept
  {
    return not m_finished;
  }
  /// Has this stream been through its concluding @c complete()?
  [[nodiscard]] constexpr bool operator!() const noexcept
  {
    return m_finished;
  }

  /// Complete the operation, and check for errors.
  /** Always call this to close the stream in an orderly fashion, even after
   * an error.  (In the case of an error, abort the transaction afterwards.)
   *
   * The only circumstance where it's safe to skip this is after an error, if
   * you're discarding the entire connection.
   */
  void complete();

  /// Insert a row of data.
  /** Returns a reference to the stream, so you can chain the calls.
   *
   * The @c row can be a tuple, or any type that can be iterated.  Each
   * item becomes a field in the row, in the same order as the columns you
   * specified when creating the stream.
   *
   * If you don't already happen to have your fields in the form of a tuple or
   * container, prefer @c write_values.  It's faster and more convenient.
   */
  template<typename Row> stream_to &operator<<(Row const &row)
  {
    write_row(row);
    return *this;
  }

  /// Stream a `stream_from` straight into a `stream_to`.
  /** This can be useful when copying between different databases.  If the
   * source and the destination are on the same database, you'll get better
   * performance doing it all in a regular query.
   */
  stream_to &operator<<(stream_from &);

  /// Insert a row of data, given in the form of a @c std::tuple or container.
  /** The @c row can be a tuple, or any type that can be iterated.  Each
   * item becomes a field in the row, in the same order as the columns you
   * specified when creating the stream.
   *
   * The preferred way to insert a row is @c write_values.
   */
  template<typename Row> void write_row(Row const &row)
  {
    fill_buffer(row);
    write_buffer();
  }

  /// Insert values as a row.
  /** This is the recommended way of inserting data.  Pass your field values,
   * of any convertible type.
   */
  template<typename... Ts> void write_values(Ts const &...fields)
  {
    fill_buffer(fields...);
    write_buffer();
  }

private:
  /// Stream a pre-quoted table name and columns list.
  stream_to(
    transaction_base &tx, std::string_view path, std::string_view columns);

  bool m_finished = false;

  /// Reusable buffer for a row.  Saves doing an allocation for each row.
  std::string m_buffer;

  /// Reusable buffer for converting/escaping a field.
  std::string m_field_buf;

  /// Glyph scanner, for parsing the client encoding.
  internal::glyph_scanner_func *m_scanner;

  /// Write a row of raw text-format data into the destination table.
  void write_raw_line(std::string_view);

  /// Write a row of data from @c m_buffer into the destination table.
  /** Resets the buffer for the next row.
   */
  void write_buffer();

  /// COPY encoding for a null field, plus subsequent separator.
  static constexpr std::string_view null_field{"\\N\t"};

  /// Estimate buffer space needed for a field which is always null.
  template<typename T>
  static std::enable_if_t<nullness<T>::always_null, std::size_t>
  estimate_buffer(T const &)
  {
    return std::size(null_field);
  }

  /// Estimate buffer space needed for field f.
  /** The estimate is not very precise.  We don't actually know how much space
   * we'll need once the escaping comes in.
   */
  template<typename T>
  static std::enable_if_t<not nullness<T>::always_null, std::size_t>
  estimate_buffer(T const &field)
  {
    return is_null(field) ? std::size(null_field) : size_buffer(field);
  }

  /// Append escaped version of @c data to @c m_buffer, plus a tab.
  void escape_field_to_buffer(std::string_view data);

  /// Append string representation for @c f to @c m_buffer.
  /** This is for the general case, where the field may contain a value.
   *
   * Also appends a tab.  The tab is meant to be a separator, not a terminator,
   * so if you write any fields at all, you'll end up with one tab too many
   * at the end of the buffer.
   */
  template<typename Field>
  std::enable_if_t<not nullness<Field>::always_null>
  append_to_buffer(Field const &f)
  {
    // We append each field, terminated by a tab.  That will leave us with
    // one tab too many, assuming we write any fields at all; we remove that
    // at the end.
    if (is_null(f))
    {
      // Easy.  Append null and tab in one go.
      m_buffer.append(null_field);
    }
    else
    {
      // Convert f into m_buffer.

      using traits = string_traits<Field>;
      auto const budget{estimate_buffer(f)};
      auto const offset{std::size(m_buffer)};

      if constexpr (std::is_arithmetic_v<Field>)
      {
        // Specially optimised for "safe" types, which never need any
        // escaping.  Convert straight into m_buffer.

        // The budget we get from size_buffer() includes room for the trailing
        // zero, which we must remove.  But we're also inserting tabs between
        // fields, so we re-purpose the extra byte for that.
        auto const total{offset + budget};
        m_buffer.resize(total);
        auto const data{m_buffer.data()};
        char *const end{traits::into_buf(data + offset, data + total, f)};
        *(end - 1) = '\t';
        // Shrink to fit.  Keep the tab though.
        m_buffer.resize(static_cast<std::size_t>(end - data));
      }
      else if constexpr (
        std::is_same_v<Field, std::string> or
        std::is_same_v<Field, std::string_view> or
        std::is_same_v<Field, zview>)
      {
        // This string may need escaping.
        m_field_buf.resize(budget);
        escape_field_to_buffer(f);
      }
      else
      {
        // This field needs to be converted to a string, and after that,
        // escaped as well.
        m_field_buf.resize(budget);
        auto const data{m_field_buf.data()};
        escape_field_to_buffer(
          traits::to_buf(data, data + std::size(m_field_buf), f));
      }
    }
  }

  /// Append string representation for a null field to @c m_buffer.
  /** This special case is for types which are always null.
   *
   * Also appends a tab.  The tab is meant to be a separator, not a terminator,
   * so if you write any fields at all, you'll end up with one tab too many
   * at the end of the buffer.
   */
  template<typename Field>
  std::enable_if_t<nullness<Field>::always_null>
  append_to_buffer(Field const &)
  {
    m_buffer.append(null_field);
  }

  /// Write raw COPY line into @c m_buffer, based on a container of fields.
  template<typename Container>
  std::enable_if_t<not std::is_same_v<typename Container::value_type, char>>
  fill_buffer(Container const &c)
  {
    // To avoid unnecessary allocations and deallocations, we run through c
    // twice: once to determine how much buffer space we may need, and once to
    // actually write it into the buffer.
    std::size_t budget{0};
    for (auto const &f : c) budget += estimate_buffer(f);
    m_buffer.reserve(budget);
    for (auto const &f : c) append_to_buffer(f);
  }

  /// Estimate how many buffer bytes we need to write tuple.
  template<typename Tuple, std::size_t... indexes>
  static std::size_t
  budget_tuple(Tuple const &t, std::index_sequence<indexes...>)
  {
    return (estimate_buffer(std::get<indexes>(t)) + ...);
  }

  /// Write tuple of fields to @c m_buffer.
  template<typename Tuple, std::size_t... indexes>
  void append_tuple(Tuple const &t, std::index_sequence<indexes...>)
  {
    (append_to_buffer(std::get<indexes>(t)), ...);
  }

  /// Write raw COPY line into @c m_buffer, based on a tuple of fields.
  template<typename... Elts> void fill_buffer(std::tuple<Elts...> const &t)
  {
    using indexes = std::make_index_sequence<sizeof...(Elts)>;

    m_buffer.reserve(budget_tuple(t, indexes{}));
    append_tuple(t, indexes{});
  }

  /// Write raw COPY line into @c m_buffer, based on varargs fields.
  template<typename... Ts> void fill_buffer(const Ts &...fields)
  {
    (..., append_to_buffer(fields));
  }

  constexpr static std::string_view s_classname{"stream_to"};
};


template<typename Columns>
inline stream_to::stream_to(
  transaction_base &tx, std::string_view table_name, Columns const &columns) :
        stream_to{tx, table_name, std::begin(columns), std::end(columns)}
{}


template<typename Iter>
inline stream_to::stream_to(
  transaction_base &tx, std::string_view table_name, Iter columns_begin,
  Iter columns_end) :
        stream_to{
          tx,
          tx.quote_name(
            table_name,
            separated_list(",", columns_begin, columns_end, [&tx](auto col) {
              return tx.quote_name(*col);
            }))}
{}
} // namespace pqxx
#endif
