/* Deprecated representation for raw, binary data.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/binarystring instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_BINARYSTRING
#define PQXX_H_BINARYSTRING

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <memory>
#include <string>
#include <string_view>

#include "pqxx/result.hxx"
#include "pqxx/strconv.hxx"

namespace pqxx
{
class binarystring;
template<> struct string_traits<binarystring>;


/// Binary data corresponding to PostgreSQL's "BYTEA" binary-string type.
/** @ingroup escaping-functions
 * @deprecated Use @c std::basic_string<std::byte> and
 * @c std::basic_string_view<std::byte> for binary data.  In C++20 or better,
 * any @c contiguous_range of @c std::byte will do.
 *
 * This class represents a binary string as stored in a field of type @c bytea.
 *
 * Internally a binarystring is zero-terminated, but it may also contain null
 * bytes, they're just like any other byte value.  So don't assume that it's
 * safe to treat the contents as a C-style string.
 *
 * The binarystring retains its value even if the result it was obtained from
 * is destroyed, but it cannot be copied or assigned.
 *
 * \relatesalso transaction_base::quote_raw
 *
 * To include a @c binarystring value in an SQL query, escape and quote it
 * using the transaction's @c quote_raw function.
 *
 * @warning This class is implemented as a reference-counting smart pointer.
 * Copying, swapping, and destroying binarystring objects that refer to the
 * same underlying data block is <em>not thread-safe</em>.  If you wish to pass
 * binarystrings around between threads, make sure that each of these
 * operations is protected against concurrency with similar operations on the
 * same object, or other objects pointing to the same data block.
 */
class PQXX_LIBEXPORT binarystring
{
public:
  using char_type = unsigned char;
  using value_type = std::char_traits<char_type>::char_type;
  using size_type = std::size_t;
  using difference_type = long;
  using const_reference = value_type const &;
  using const_pointer = value_type const *;
  using const_iterator = const_pointer;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  [[deprecated("Use std::byte for binary data.")]] binarystring(
    binarystring const &) = default;

  /// Read and unescape bytea field.
  /** The field will be zero-terminated, even if the original bytea field
   * isn't.
   * @param F the field to read; must be a bytea field
   */
  [[deprecated("Use std::byte for binary data.")]] explicit binarystring(
    field const &);

  /// Copy binary data from std::string_view on binary data.
  /** This is inefficient in that it copies the data to a buffer allocated on
   * the heap.
   */
  [[deprecated("Use std::byte for binary data.")]] explicit binarystring(
    std::string_view);

  /// Copy binary data of given length straight out of memory.
  [[deprecated("Use std::byte for binary data.")]] binarystring(
    void const *, std::size_t);

  /// Efficiently wrap a buffer of binary data in a @c binarystring.
  [[deprecated("Use std::byte for binary data.")]] binarystring(
    std::shared_ptr<value_type> ptr, size_type size) :
          m_buf{std::move(ptr)}, m_size{size}
  {}

  /// Size of converted string in bytes.
  [[nodiscard]] size_type size() const noexcept { return m_size; }
  /// Size of converted string in bytes.
  [[nodiscard]] size_type length() const noexcept { return size(); }
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  [[nodiscard]] const_iterator begin() const noexcept { return data(); }
  [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
  [[nodiscard]] const_iterator end() const noexcept { return data() + m_size; }
  [[nodiscard]] const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] const_reference front() const noexcept { return *begin(); }
  [[nodiscard]] const_reference back() const noexcept
  {
    return *(data() + m_size - 1);
  }

  [[nodiscard]] const_reverse_iterator rbegin() const
  {
    return const_reverse_iterator{end()};
  }
  [[nodiscard]] const_reverse_iterator crbegin() const { return rbegin(); }
  [[nodiscard]] const_reverse_iterator rend() const
  {
    return const_reverse_iterator{begin()};
  }
  [[nodiscard]] const_reverse_iterator crend() const { return rend(); }

  /// Unescaped field contents.
  [[nodiscard]] value_type const *data() const noexcept { return m_buf.get(); }

  [[nodiscard]] const_reference operator[](size_type i) const noexcept
  {
    return data()[i];
  }

  [[nodiscard]] PQXX_PURE bool operator==(binarystring const &) const noexcept;
  [[nodiscard]] bool operator!=(binarystring const &rhs) const noexcept
  {
    return not operator==(rhs);
  }

  binarystring &operator=(binarystring const &);

  /// Index contained string, checking for valid index.
  const_reference at(size_type) const;

  /// Swap contents with other binarystring.
  void swap(binarystring &);

  /// Raw character buffer (no terminating zero is added).
  /** @warning No terminating zero is added!  If the binary data did not end in
   * a null character, you will not find one here.
   */
  [[nodiscard]] char const *get() const noexcept
  {
    return reinterpret_cast<char const *>(m_buf.get());
  }

  /// Read contents as a std::string_view.
  [[nodiscard]] std::string_view view() const noexcept
  {
    return std::string_view(get(), size());
  }

  /// Read as regular C++ string (may include null characters).
  /** This creates and returns a new string object.  Don't call this
   * repeatedly; retrieve your string once and keep it in a local variable.
   * Also, do not expect to be able to compare the string's address to that of
   * an earlier invocation.
   */
  [[nodiscard]] std::string str() const;

  /// Access data as a pointer to @c std::byte.
  [[nodiscard]] std::byte const *bytes() const
  {
    return reinterpret_cast<std::byte const *>(get());
  }

  /// Read data as a @c std::basic_string_view<std::byte>.
  [[nodiscard]] std::basic_string_view<std::byte> bytes_view() const
  {
    return std::basic_string_view<std::byte>{bytes(), size()};
  }

private:
  std::shared_ptr<value_type> m_buf;
  size_type m_size{0};
};


template<> struct nullness<binarystring> : no_null<binarystring>
{};


/// String conversion traits for @c binarystring.
/** Defines the conversions between a @c binarystring and its PostgreSQL
 * textual format, for communication with the database.
 *
 * These conversions rely on the "hex" format which was introduced in
 * PostgreSQL 9.0.  Both your libpq and the server must be recent enough to
 * speak this format.
 */
template<> struct string_traits<binarystring>
{
  static std::size_t size_buffer(binarystring const &value) noexcept
  {
    return internal::size_esc_bin(std::size(value));
  }

  static zview to_buf(char *begin, char *end, binarystring const &value)
  {
    return generic_to_buf(begin, end, value);
  }

  static char *into_buf(char *begin, char *end, binarystring const &value)
  {
    auto const budget{size_buffer(value)};
    if (internal::cmp_less(end - begin, budget))
      throw conversion_overrun{
        "Not enough buffer space to escape binary data."};
    std::string_view text{value.view()};
    internal::esc_bin(binary_cast(text), begin);
    return begin + budget;
  }

  static binarystring from_string(std::string_view text)
  {
    auto const size{pqxx::internal::size_unesc_bin(std::size(text))};
    std::shared_ptr<unsigned char> buf{
      new unsigned char[size], [](unsigned char const *x) { delete[] x; }};
    pqxx::internal::unesc_bin(text, reinterpret_cast<std::byte *>(buf.get()));
#include "pqxx/internal/ignore-deprecated-pre.hxx"
    return binarystring{std::move(buf), size};
#include "pqxx/internal/ignore-deprecated-post.hxx"
  }
};
} // namespace pqxx
#endif
