/* Definitions for the pqxx::field class.
 *
 * pqxx::field refers to a field in a query result.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/field instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_FIELD
#define PQXX_H_FIELD

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <optional>

#include "pqxx/array.hxx"
#include "pqxx/composite.hxx"
#include "pqxx/result.hxx"
#include "pqxx/strconv.hxx"
#include "pqxx/types.hxx"

namespace pqxx
{
/// Reference to a field in a result set.
/** A field represents one entry in a row.  It represents an actual value
 * in the result set, and can be converted to various types.
 */
class PQXX_LIBEXPORT field
{
public:
  using size_type = field_size_type;

  /// Constructor.  Do not call this yourself; libpqxx will do it for you.
  /** Create field as reference to a field in a result set.
   * @param r Row that this field is part of.
   * @param c Column number of this field.
   */
  [[deprecated(
    "Do not construct fields yourself.  Get them from the row.")]] field(row const &r, row_size_type c) noexcept;

  /// Constructor.  Do not call this yourself; libpqxx will do it for you.
  [[deprecated(
    "Do not construct fields yourself.  Get them from the "
    "row.")]] field() noexcept = default;

  /**
   * @name Comparison
   */
  //@{
  // TODO: noexcept.  Breaks ABI.
  /// Byte-by-byte comparison of two fields (all nulls are considered equal)
  /** @warning null handling is still open to discussion and change!
   *
   * Handling of null values differs from that in SQL where a comparison
   * involving a null value yields null, so nulls are never considered equal
   * to one another or even to themselves.
   *
   * Null handling also probably differs from the closest equivalent in C++,
   * which is the NaN (Not-a-Number) value, a singularity comparable to
   * SQL's null.  This is because the builtin == operator demands that a == a.
   *
   * The usefulness of this operator is questionable.  No interpretation
   * whatsoever is imposed on the data; 0 and 0.0 are considered different,
   * as are null vs. the empty string, or even different (but possibly
   * equivalent and equally valid) encodings of the same Unicode character
   * etc.
   */
  [[nodiscard]] PQXX_PURE bool operator==(field const &) const;

  /// Byte-by-byte comparison (all nulls are considered equal)
  /** @warning See operator==() for important information about this operator
   */
  [[nodiscard]] PQXX_PURE bool operator!=(field const &rhs) const noexcept
  {
    return not operator==(rhs);
  }
  //@}

  /**
   * @name Column information
   */
  //@{
  /// Column name.
  [[nodiscard]] PQXX_PURE char const *name() const &;

  /// Column type.
  [[nodiscard]] oid PQXX_PURE type() const;

  /// What table did this column come from?
  [[nodiscard]] PQXX_PURE oid table() const;

  /// Return row number.  The first row is row 0, the second is row 1, etc.
  PQXX_PURE constexpr row_size_type num() const noexcept { return col(); }

  /// What column number in its originating table did this column come from?
  [[nodiscard]] PQXX_PURE row_size_type table_column() const;
  //@}

  /**
   * @name Content access
   */
  //@{
  /// Read as `string_view`, or an empty one if null.
  /** The result only remains usable while the data for the underlying
   * @ref result exists.  Once all `result` objects referring to that data have
   * been destroyed, the `string_view` will no longer point to valid memory.
   */
  [[nodiscard]] PQXX_PURE std::string_view view() const &
  {
    return std::string_view(c_str(), size());
  }

  /// Read as plain C string.
  /** Since the field's data is stored internally in the form of a
   * zero-terminated C string, this is the fastest way to read it.  Use the
   * to() or as() functions to convert the string to other types such as
   * `int`, or to C++ strings.
   *
   * Do not use this for BYTEA values, or other binary values.  To read those,
   * convert the value to your desired type using `to()` or `as()`.  For
   * example: `f.as<std::basic_string<std::byte>>()`.
   */
  [[nodiscard]] PQXX_PURE char const *c_str() const &;

  /// Is this field's value null?
  [[nodiscard]] PQXX_PURE bool is_null() const noexcept;

  /// Return number of bytes taken up by the field's value.
  [[nodiscard]] PQXX_PURE size_type size() const noexcept;

  /// Read value into obj; or if null, leave obj untouched and return `false`.
  /** This can be used with optional types (except pointers other than C-style
   * strings).
   */
  template<typename T>
  auto to(T &obj) const -> typename std::enable_if_t<
    (not std::is_pointer<T>::value or std::is_same<T, char const *>::value),
    bool>
  {
    if (is_null())
    {
      return false;
    }
    else
    {
      auto const bytes{c_str()};
      from_string(bytes, obj);
      return true;
    }
  }

  /// Read field as a composite value, write its components into `fields`.
  /** @warning This is still experimental.  It may change or be replaced.
   *
   * Returns whether the field was null.  If it was, it will not touch the
   * values in `fields`.
   */
  template<typename... T> bool composite_to(T &...fields) const
  {
    if (is_null())
    {
      return false;
    }
    else
    {
      parse_composite(m_home.m_encoding, view(), fields...);
      return true;
    }
  }

  /// Read value into obj; or leave obj untouched and return `false` if null.
  template<typename T> bool operator>>(T &obj) const { return to(obj); }

  /// Read value into obj; or if null, use default value and return `false`.
  /** This can be used with `std::optional`, as well as with standard smart
   * pointer types, but not with raw pointers.  If the conversion from a
   * PostgreSQL string representation allocates a pointer (e.g. using `new`),
   * then the object's later deallocation should be baked in as well, right
   * from the point where the object is created.  So if you want a pointer, use
   * a smart pointer, not a raw pointer.
   *
   * There is one exception, of course: C-style strings.  Those are just
   * pointers to the field's internal text data.
   */
  template<typename T>
  auto to(T &obj, T const &default_value) const -> typename std::enable_if_t<
    (not std::is_pointer<T>::value or std::is_same<T, char const *>::value),
    bool>
  {
    bool const null{is_null()};
    if (null)
      obj = default_value;
    else
      obj = from_string<T>(this->view());
    return not null;
  }

  /// Return value as object of given type, or default value if null.
  /** Note that unless the function is instantiated with an explicit template
   * argument, the Default value's type also determines the result type.
   */
  template<typename T> T as(T const &default_value) const
  {
    if (is_null())
      return default_value;
    else
      return from_string<T>(this->view());
  }

  /// Return value as object of given type, or throw exception if null.
  /** Use as `as<std::optional<int>>()` or `as<my_untemplated_optional_t>()` as
   * an alternative to `get<int>()`; this is disabled for use with raw pointers
   * (other than C-strings) because storage for the value can't safely be
   * allocated here
   */
  template<typename T> T as() const
  {
    if (is_null())
    {
      if constexpr (not nullness<T>::has_null)
        internal::throw_null_conversion(type_name<T>);
      else
        return nullness<T>::null();
    }
    else
    {
      return from_string<T>(this->view());
    }
  }

  /// Return value wrapped in some optional type (empty for nulls).
  /** Use as `get<int>()` as before to obtain previous behavior, or specify
   * container type with `get<int, std::optional>()`
   */
  template<typename T, template<typename> class O = std::optional>
  constexpr O<T> get() const
  {
    return as<O<T>>();
  }

  // TODO: constexpr noexcept, once array_parser constructor gets those.
  /// Parse the field as an SQL array.
  /** Call the parser to retrieve values (and structure) from the array.
   *
   * Make sure the @ref result object stays alive until parsing is finished. If
   * you keep the @ref row of `field` object alive, it will keep the @ref
   * result object alive as well.
   */
  array_parser as_array() const &
  {
    return array_parser{c_str(), m_home.m_encoding};
  }
  //@}


protected:
  constexpr result const &home() const noexcept { return m_home; }
  constexpr result::size_type idx() const noexcept { return m_row; }
  constexpr row_size_type col() const noexcept { return m_col; }

  // TODO: Create gates.
  friend class pqxx::result;
  friend class pqxx::row;
  field(
    result const &r, result_size_type row_num, row_size_type col_num) noexcept
          :
          m_col{col_num}, m_home{r}, m_row{row_num}
  {}

  /**
   * You'd expect this to be unsigned, but due to the way reverse iterators
   * are related to regular iterators, it must be allowed to underflow to -1.
   */
  row_size_type m_col;

private:
  result m_home;
  result::size_type m_row;
};


template<> inline bool field::to<std::string>(std::string &obj) const
{
  bool const null{is_null()};
  if (not null)
    obj = std::string{view()};
  return not null;
}


template<>
inline bool field::to<std::string>(
  std::string &obj, std::string const &default_value) const
{
  bool const null{is_null()};
  if (null)
    obj = default_value;
  else
    obj = std::string{view()};
  return not null;
}


/// Specialization: `to(char const *&)`.
/** The buffer has the same lifetime as the data in this result (i.e. of this
 * result object, or the last remaining one copied from it etc.), so take care
 * not to use it after the last result object referring to this query result is
 * destroyed.
 */
template<> inline bool field::to<char const *>(char const *&obj) const
{
  bool const null{is_null()};
  if (not null)
    obj = c_str();
  return not null;
}


template<> inline bool field::to<std::string_view>(std::string_view &obj) const
{
  bool const null{is_null()};
  if (not null)
    obj = view();
  return not null;
}


template<>
inline bool field::to<std::string_view>(
  std::string_view &obj, std::string_view const &default_value) const
{
  bool const null{is_null()};
  if (null)
    obj = default_value;
  else
    obj = view();
  return not null;
}


template<> inline std::string_view field::as<std::string_view>() const
{
  if (is_null())
    PQXX_UNLIKELY
  internal::throw_null_conversion(type_name<std::string_view>);
  return view();
}


template<>
inline std::string_view
field::as<std::string_view>(std::string_view const &default_value) const
{
  return is_null() ? default_value : view();
}


template<> inline bool field::to<zview>(zview &obj) const
{
  bool const null{is_null()};
  if (not null)
    obj = zview{c_str(), size()};
  return not null;
}


template<>
inline bool field::to<zview>(zview &obj, zview const &default_value) const
{
  bool const null{is_null()};
  if (null)
    obj = default_value;
  else
    obj = zview{c_str(), size()};
  return not null;
}


template<> inline zview field::as<zview>() const
{
  if (is_null())
    PQXX_UNLIKELY
  internal::throw_null_conversion(type_name<zview>);
  return zview{c_str(), size()};
}


template<> inline zview field::as<zview>(zview const &default_value) const
{
  return is_null() ? default_value : zview{c_str(), size()};
}


template<typename CHAR = char, typename TRAITS = std::char_traits<CHAR>>
class field_streambuf : public std::basic_streambuf<CHAR, TRAITS>
{
public:
  using char_type = CHAR;
  using traits_type = TRAITS;
  using int_type = typename traits_type::int_type;
  using pos_type = typename traits_type::pos_type;
  using off_type = typename traits_type::off_type;
  using openmode = std::ios::openmode;
  using seekdir = std::ios::seekdir;

  explicit field_streambuf(field const &f) : m_field{f} { initialize(); }

protected:
  virtual int sync() override { return traits_type::eof(); }

  virtual pos_type seekoff(off_type, seekdir, openmode) override
  {
    return traits_type::eof();
  }
  virtual pos_type seekpos(pos_type, openmode) override
  {
    return traits_type::eof();
  }
  virtual int_type overflow(int_type) override { return traits_type::eof(); }
  virtual int_type underflow() override { return traits_type::eof(); }

private:
  field const &m_field;

  int_type initialize()
  {
    auto g{static_cast<char_type *>(const_cast<char *>(m_field.c_str()))};
    this->setg(g, g, g + std::size(m_field));
    return int_type(std::size(m_field));
  }
};


/// Input stream that gets its data from a result field
/** Use this class exactly as you would any other istream to read data from a
 * field.  All formatting and streaming operations of `std::istream` are
 * supported.  What you'll typically want to use, however, is the fieldstream
 * alias (which defines a @ref basic_fieldstream for `char`).  This is similar
 * to how e.g. `std::ifstream` relates to `std::basic_ifstream`.
 *
 * This class has only been tested for the char type (and its default traits).
 */
template<typename CHAR = char, typename TRAITS = std::char_traits<CHAR>>
class basic_fieldstream : public std::basic_istream<CHAR, TRAITS>
{
  using super = std::basic_istream<CHAR, TRAITS>;

public:
  using char_type = CHAR;
  using traits_type = TRAITS;
  using int_type = typename traits_type::int_type;
  using pos_type = typename traits_type::pos_type;
  using off_type = typename traits_type::off_type;

  basic_fieldstream(field const &f) : super{nullptr}, m_buf{f}
  {
    super::init(&m_buf);
  }

private:
  field_streambuf<CHAR, TRAITS> m_buf;
};

using fieldstream = basic_fieldstream<char>;

/// Write a result field to any type of stream
/** This can be convenient when writing a field to an output stream.  More
 * importantly, it lets you write a field to e.g. a `stringstream` which you
 * can then use to read, format and convert the field in ways that to() does
 * not support.
 *
 * Example: parse a field into a variable of the nonstandard
 * "<tt>long long</tt>" type.
 *
 * ```cxx
 * extern result R;
 * long long L;
 * stringstream S;
 *
 * // Write field's string into S
 * S << R[0][0];
 *
 * // Parse contents of S into L
 * S >> L;
 * ```
 */
template<typename CHAR>
inline std::basic_ostream<CHAR> &
operator<<(std::basic_ostream<CHAR> &s, field const &value)
{
  s.write(value.c_str(), std::streamsize(std::size(value)));
  return s;
}


/// Convert a field's value to type `T`.
/** Unlike the "regular" `from_string`, this knows how to deal with null
 * values.
 */
template<typename T> inline T from_string(field const &value)
{
  if (value.is_null())
  {
    if constexpr (nullness<T>::has_null)
      return nullness<T>::null();
    else
      internal::throw_null_conversion(type_name<T>);
  }
  else
  {
    return from_string<T>(value.view());
  }
}


/// Convert a field's value to `nullptr_t`.
/** Yes, you read that right.  This conversion does nothing useful.  It always
 * returns `nullptr`.
 *
 * Except... what if the field is not null?  In that case, this throws
 * @ref conversion_error.
 */
template<>
inline std::nullptr_t from_string<std::nullptr_t>(field const &value)
{
  if (not value.is_null())
    throw conversion_error{
      "Extracting non-null field into nullptr_t variable."};
  return nullptr;
}


/// Convert a field to a string.
template<> PQXX_LIBEXPORT std::string to_string(field const &value);
} // namespace pqxx
#endif
