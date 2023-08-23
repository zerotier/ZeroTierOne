/* Definitions for the pqxx::result class and support classes.
 *
 * pqxx::result represents the set of result rows from a database query.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/result instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_RESULT
#define PQXX_H_RESULT

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <functional>
#include <ios>
#include <memory>
#include <stdexcept>

#include "pqxx/except.hxx"
#include "pqxx/types.hxx"
#include "pqxx/util.hxx"
#include "pqxx/zview.hxx"

#include "pqxx/internal/encodings.hxx"


namespace pqxx::internal
{
// TODO: Make noexcept (but breaks ABI).
PQXX_LIBEXPORT void clear_result(pq::PGresult const *);
} // namespace pqxx::internal


namespace pqxx::internal::gate
{
class result_connection;
class result_creation;
class result_pipeline;
class result_row;
class result_sql_cursor;
} // namespace pqxx::internal::gate


namespace pqxx
{
/// Result set containing data returned by a query or command.
/** This behaves as a container (as defined by the C++ standard library) and
 * provides random access const iterators to iterate over its rows.  You can
 * also access a row by indexing a `result R` by the row's zero-based
 * number:
 *
 *
 *	for (result::size_type i=0; i < std::size(R); ++i) Process(R[i]);
 *
 *
 * Result sets in libpqxx are lightweight, reference-counted wrapper objects
 * which are relatively small and cheap to copy.  Think of a result object as
 * a "smart pointer" to an underlying result set.
 *
 * @warning The result set that a result object points to is not thread-safe.
 * If you copy a result object, it still refers to the same underlying result
 * set.  So never copy, destroy, query, or otherwise access a result while
 * another thread may be copying, destroying, querying, or otherwise accessing
 * the same result set--even if it is doing so through a different result
 * object!
 */
class PQXX_LIBEXPORT result
{
public:
  using size_type = result_size_type;
  using difference_type = result_difference_type;
  using reference = row;
  using const_iterator = const_result_iterator;
  using pointer = const_iterator;
  using iterator = const_iterator;
  using const_reverse_iterator = const_reverse_result_iterator;
  using reverse_iterator = const_reverse_iterator;

  result() noexcept :
          m_data{make_data_pointer()},
          m_query{},
          m_encoding{internal::encoding_group::MONOBYTE}
  {}

  result(result const &rhs) noexcept = default;
  result(result &&rhs) noexcept = default;

  /// Assign one result to another.
  /** Copying results is cheap: it copies only smart pointers, but the actual
   * data stays in the same place.
   */
  result &operator=(result const &rhs) noexcept = default;

  /// Assign one result to another, invaliding the old one.
  result &operator=(result &&rhs) noexcept = default;

  /**
   * @name Comparisons
   *
   * You can compare results for equality.  Beware: this is a very strict,
   * dumb comparison.  The smallest difference between two results (such as a
   * string "Foo" versus a string "foo") will make them unequal.
   */
  //@{
  /// Compare two results for equality.
  [[nodiscard]] bool operator==(result const &) const noexcept;
  /// Compare two results for inequality.
  [[nodiscard]] bool operator!=(result const &rhs) const noexcept
  {
    return not operator==(rhs);
  }
  //@}

  /// Iterate rows, reading them directly into a tuple of "TYPE...".
  /** Converts the fields to values of the given respective types.
   *
   * Use this only with a ranged "for" loop.  The iteration produces
   * std::tuple<TYPE...> which you can "unpack" to a series of `auto`
   * variables.
   */
  template<typename... TYPE> auto iter() const;

  [[nodiscard]] const_reverse_iterator rbegin() const;
  [[nodiscard]] const_reverse_iterator crbegin() const;
  [[nodiscard]] const_reverse_iterator rend() const;
  [[nodiscard]] const_reverse_iterator crend() const;

  [[nodiscard]] const_iterator begin() const noexcept;
  [[nodiscard]] const_iterator cbegin() const noexcept;
  [[nodiscard]] inline const_iterator end() const noexcept;
  [[nodiscard]] inline const_iterator cend() const noexcept;

  [[nodiscard]] reference front() const noexcept;
  [[nodiscard]] reference back() const noexcept;

  [[nodiscard]] PQXX_PURE size_type size() const noexcept;
  [[nodiscard]] PQXX_PURE bool empty() const noexcept;
  [[nodiscard]] size_type capacity() const noexcept { return size(); }

  /// Exchange two `result` values in an exception-safe manner.
  /** If the swap fails, the two values will be exactly as they were before.
   *
   * The swap is not necessarily thread-safe.
   */
  void swap(result &) noexcept;

  /// Index a row by number.
  /** This returns a @ref row object.  Generally you should not keep the row
   * around as a variable, but if you do, make sure that your variable is a
   * `row`, not a `row&`.
   */
  [[nodiscard]] row operator[](size_type i) const noexcept;

#if defined(PQXX_HAVE_MULTIDIMENSIONAL_SUBSCRIPT)
  // TODO: If C++23 will let us, also accept string for the column.
  [[nodiscard]] field
  operator[](size_type row_num, row_size_type col_num) const noexcept;
#endif

  /// Index a row by number, but check that the row number is valid.
  row at(size_type) const;

  /// Index a field by row number and column number.
  field at(size_type, row_size_type) const;

  /// Let go of the result's data.
  /** Use this if you need to deallocate the result data earlier than you can
   * destroy the `result` object itself.
   *
   * Multiple `result` objects can refer to the same set of underlying data.
   * The underlying data will be deallocated once all `result` objects that
   * refer to it are cleared or destroyed.
   */
  void clear() noexcept
  {
    m_data.reset();
    m_query = nullptr;
  }

  /**
   * @name Column information
   */
  //@{
  /// Number of columns in result.
  [[nodiscard]] PQXX_PURE row_size_type columns() const noexcept;

  /// Number of given column (throws exception if it doesn't exist).
  [[nodiscard]] row_size_type column_number(zview name) const;

  /// Name of column with this number (throws exception if it doesn't exist)
  [[nodiscard]] char const *column_name(row_size_type number) const &;

  /// Return column's type, as an OID from the system catalogue.
  [[nodiscard]] oid column_type(row_size_type col_num) const;

  /// Return column's type, as an OID from the system catalogue.
  [[nodiscard]] oid column_type(zview col_name) const
  {
    return column_type(column_number(col_name));
  }

  /// What table did this column come from?
  [[nodiscard]] oid column_table(row_size_type col_num) const;

  /// What table did this column come from?
  [[nodiscard]] oid column_table(zview col_name) const
  {
    return column_table(column_number(col_name));
  }

  /// What column in its table did this column come from?
  [[nodiscard]] row_size_type table_column(row_size_type col_num) const;

  /// What column in its table did this column come from?
  [[nodiscard]] row_size_type table_column(zview col_name) const
  {
    return table_column(column_number(col_name));
  }
  //@}

  /// Query that produced this result, if available (empty string otherwise)
  [[nodiscard]] PQXX_PURE std::string const &query() const &noexcept;

  /// If command was an `INSERT` of 1 row, return oid of the inserted row.
  /** @return Identifier of inserted row if exactly one row was inserted, or
   * @ref oid_none otherwise.
   */
  [[nodiscard]] PQXX_PURE oid inserted_oid() const;

  /// If command was `INSERT`, `UPDATE`, or `DELETE`: number of affected rows.
  /** @return Number of affected rows if last command was `INSERT`, `UPDATE`,
   * or `DELETE`; zero for all other commands.
   */
  [[nodiscard]] PQXX_PURE size_type affected_rows() const;

  // C++20: Concept like std::invocable, but without specifying param types.
  /// Run `func` on each row, passing the row's fields as parameters.
  /** Goes through the rows from first to last.  You provide a callable `func`.
   *
   * For each row in the `result`, `for_each` will call `func`.  It converts
   * the row's fields to the types of `func`'s parameters, and pass them to
   * `func`.
   *
   * (Therefore `func` must have a _single_ signature.  It can't be a generic
   * lambda, or an object of a class with multiple overloaded function call
   * operators.  Otherwise, `for_each` will have no way to detect a parameter
   * list without ambiguity.)
   *
   * If any of your parameter types is `std::string_view`, it refers to the
   * underlying storage of this `result`.
   *
   * If any of your parameter types is a reference type, its argument will
   * refer to a temporary value which only lives for the duration of that
   * single invocation to `func`.  If the reference is an lvalue reference, it
   * must be `const`.
   *
   * For example, this queries employee names and salaries from the database
   * and prints how much each would like to earn instead:
   * ```cxx
   *   tx.exec("SELECT name, salary FROM employee").for_each(
   *       [](std::string_view name, float salary){
   *           std::cout << name << " would like " << salary * 2 << ".\n";
   *       })
   * ```
   *
   * If `func` throws an exception, processing stops at that point and
   * propagates the exception.
   *
   * @throws usage_error if `func`'s number of parameters does not match the
   * number of columns in this result.
   */
  template<typename CALLABLE> inline void for_each(CALLABLE &&func) const;

private:
  using data_pointer = std::shared_ptr<internal::pq::PGresult const>;

  /// Underlying libpq result set.
  data_pointer m_data;

  /// Factory for data_pointer.
  static data_pointer
  make_data_pointer(internal::pq::PGresult const *res = nullptr) noexcept
  {
    return {res, internal::clear_result};
  }

  friend class pqxx::internal::gate::result_pipeline;
  PQXX_PURE std::shared_ptr<std::string const> query_ptr() const noexcept
  {
    return m_query;
  }

  /// Query string.
  std::shared_ptr<std::string const> m_query;

  internal::encoding_group m_encoding;

  static std::string const s_empty_string;

  friend class pqxx::field;
  // TODO: noexcept.  Breaks ABI.
  PQXX_PURE char const *get_value(size_type row, row_size_type col) const;
  // TODO: noexcept.  Breaks ABI.
  PQXX_PURE bool get_is_null(size_type row, row_size_type col) const;
  PQXX_PURE
  field_size_type get_length(size_type, row_size_type) const noexcept;

  friend class pqxx::internal::gate::result_creation;
  result(
    internal::pq::PGresult *rhs, std::shared_ptr<std::string> query,
    internal::encoding_group enc);

  PQXX_PRIVATE void check_status(std::string_view desc = ""sv) const;

  friend class pqxx::internal::gate::result_connection;
  friend class pqxx::internal::gate::result_row;
  bool operator!() const noexcept { return m_data.get() == nullptr; }
  operator bool() const noexcept { return m_data.get() != nullptr; }

  [[noreturn]] PQXX_PRIVATE void
  throw_sql_error(std::string const &Err, std::string const &Query) const;
  PQXX_PRIVATE PQXX_PURE int errorposition() const;
  PQXX_PRIVATE std::string status_error() const;

  friend class pqxx::internal::gate::result_sql_cursor;
  PQXX_PURE char const *cmd_status() const noexcept;
};
} // namespace pqxx
#endif
