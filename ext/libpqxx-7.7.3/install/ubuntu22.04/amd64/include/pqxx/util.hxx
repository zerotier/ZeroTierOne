/* Various utility definitions for libpqxx.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/util instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_UTIL
#define PQXX_H_UTIL

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <cctype>
#include <cstdio>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#if __has_include(<version>)
#  include <version>
#endif

#include "pqxx/except.hxx"
#include "pqxx/internal/encodings.hxx"
#include "pqxx/types.hxx"
#include "pqxx/version.hxx"


/// The home of all libpqxx classes, functions, templates, etc.
namespace pqxx
{}

#include <pqxx/internal/libpq-forward.hxx>


/// Internal items for libpqxx' own use.  Do not use these yourself.
namespace pqxx::internal
{

// C++20: Retire wrapper.
/// Same as `std::cmp_less`, or a workaround where that's not available.
template<typename LEFT, typename RIGHT>
inline constexpr bool cmp_less(LEFT lhs, RIGHT rhs) noexcept
{
#if defined(PQXX_HAVE_CMP)
  return std::cmp_less(lhs, rhs);
#else
  // We need a variable just because lgtm.com gives off a false positive
  // warning when we compare the values directly.  It considers that a
  // "self-comparison."
  constexpr bool left_signed{std::is_signed_v<LEFT>};
  if constexpr (left_signed == std::is_signed_v<RIGHT>)
    return lhs < rhs;
  else if constexpr (std::is_signed_v<LEFT>)
    return (lhs <= 0) ? true : (std::make_unsigned_t<LEFT>(lhs) < rhs);
  else
    return (rhs <= 0) ? false : (lhs < std::make_unsigned_t<RIGHT>(rhs));
#endif
}


// C++20: Retire wrapper.
/// C++20 std::cmp_greater, or workaround if not available.
template<typename LEFT, typename RIGHT>
inline constexpr bool cmp_greater(LEFT lhs, RIGHT rhs) noexcept
{
#if defined(PQXX_HAVE_CMP)
  return std::cmp_greater(lhs, rhs);
#else
  return cmp_less(rhs, lhs);
#endif
}


// C++20: Retire wrapper.
/// C++20 std::cmp_less_equal, or workaround if not available.
template<typename LEFT, typename RIGHT>
inline constexpr bool cmp_less_equal(LEFT lhs, RIGHT rhs) noexcept
{
#if defined(PQXX_HAVE_CMP)
  return std::cmp_less_equal(lhs, rhs);
#else
  return not cmp_less(rhs, lhs);
#endif
}


// C++20: Retire wrapper.
/// C++20 std::cmp_greater_equal, or workaround if not available.
template<typename LEFT, typename RIGHT>
inline constexpr bool cmp_greater_equal(LEFT lhs, RIGHT rhs) noexcept
{
#if defined(PQXX_HAVE_CMP)
  return std::cmp_greater_equal(lhs, rhs);
#else
  return not cmp_less(lhs, rhs);
#endif
}


/// Efficiently concatenate two strings.
/** This is a special case of concatenate(), needed because dependency
 * management does not let us use that function here.
 */
[[nodiscard]] inline std::string cat2(std::string_view x, std::string_view y)
{
  std::string buf;
  auto const xs{std::size(x)}, ys{std::size(y)};
  buf.resize(xs + ys);
  x.copy(std::data(buf), xs);
  y.copy(std::data(buf) + xs, ys);
  return buf;
}
} // namespace pqxx::internal


namespace pqxx
{
using namespace std::literals;

/// Suppress compiler warning about an unused item.
template<typename... T> inline constexpr void ignore_unused(T &&...) noexcept
{}


/// Cast a numeric value to another type, or throw if it underflows/overflows.
/** Both types must be arithmetic types, and they must either be both integral
 * or both floating-point types.
 */
template<typename TO, typename FROM>
inline TO check_cast(FROM value, std::string_view description)
{
  static_assert(std::is_arithmetic_v<FROM>);
  static_assert(std::is_arithmetic_v<TO>);
  static_assert(std::is_integral_v<FROM> == std::is_integral_v<TO>);

  // The rest of this code won't quite work for bool, but bool is trivially
  // convertible to other arithmetic types as far as I can see.
  if constexpr (std::is_same_v<FROM, bool>)
    return static_cast<TO>(value);

  // Depending on our "if constexpr" conditions, this parameter may not be
  // needed.  Some compilers will warn.
  ignore_unused(description);

  using from_limits = std::numeric_limits<decltype(value)>;
  using to_limits = std::numeric_limits<TO>;
  if constexpr (std::is_signed_v<FROM>)
  {
    if constexpr (std::is_signed_v<TO>)
    {
      if (value < to_limits::lowest())
        throw range_error{internal::cat2("Cast underflow: "sv, description)};
    }
    else
    {
      // FROM is signed, but TO is not.  Treat this as a special case, because
      // there may not be a good broader type in which the compiler can even
      // perform our check.
      if (value < 0)
        throw range_error{internal::cat2(
          "Casting negative value to unsigned type: "sv, description)};
    }
  }
  else
  {
    // No need to check: the value is unsigned so can't fall below the range
    // of the TO type.
  }

  if constexpr (std::is_integral_v<FROM>)
  {
    using unsigned_from = std::make_unsigned_t<FROM>;
    using unsigned_to = std::make_unsigned_t<TO>;
    constexpr auto from_max{static_cast<unsigned_from>((from_limits::max)())};
    constexpr auto to_max{static_cast<unsigned_to>((to_limits::max)())};
    if constexpr (from_max > to_max)
    {
      if (internal::cmp_greater(value, to_max))
        throw range_error{internal::cat2("Cast overflow: "sv, description)};
    }
  }
  else if constexpr ((from_limits::max)() > (to_limits::max)())
  {
    if (value > (to_limits::max)())
      throw range_error{internal::cat2("Cast overflow: ", description)};
  }

  return static_cast<TO>(value);
}


/** Check library version at link time.
 *
 * Ensures a failure when linking an application against a radically
 * different libpqxx version than the one against which it was compiled.
 *
 * Sometimes application builds fail in unclear ways because they compile
 * using headers from libpqxx version X, but then link against libpqxx
 * binary version Y.  A typical scenario would be one where you're building
 * against a libpqxx which you have built yourself, but a different version
 * is installed on the system.
 *
 * The check_library_version template is declared for any library version,
 * but only actually defined for the version of the libpqxx binary against
 * which the code is linked.
 *
 * If the library binary is a different version than the one declared in
 * these headers, then this call will fail to link: there will be no
 * definition for the function with these exact template parameter values.
 * There will be a definition, but the version in the parameter values will
 * be different.
 */
inline PQXX_PRIVATE void check_version() noexcept
{
  // There is no particular reason to do this here in @ref connection, except
  // to ensure that every meaningful libpqxx client will execute it.  The call
  // must be in the execution path somewhere or the compiler won't try to link
  // it.  We can't use it to initialise a global or class-static variable,
  // because a smart compiler might resolve it at compile time.
  //
  // On the other hand, we don't want to make a useless function call too
  // often for performance reasons.  A local static variable is initialised
  // only on the definition's first execution.  Compilers will be well
  // optimised for this behaviour, so there's a minimal one-time cost.
  static auto const version_ok{internal::PQXX_VERSION_CHECK()};
  ignore_unused(version_ok);
}


/// Descriptor of library's thread-safety model.
/** This describes what the library knows about various risks to thread-safety.
 */
struct PQXX_LIBEXPORT thread_safety_model
{
  /// Is the underlying libpq build thread-safe?
  bool safe_libpq = false;

  /// Is Kerberos thread-safe?
  /** @warning Is currently always `false`.
   *
   * If your application uses Kerberos, all accesses to libpqxx or Kerberos
   * must be serialized.  Confine their use to a single thread, or protect it
   * with a global lock.
   */
  bool safe_kerberos = false;

  /// A human-readable description of any thread-safety issues.
  std::string description;
};


/// Describe thread safety available in this build.
[[nodiscard]] PQXX_LIBEXPORT thread_safety_model describe_thread_safety();


#if defined(PQXX_HAVE_CONCEPTS)
#  define PQXX_POTENTIAL_BINARY_ARG pqxx::potential_binary
#else
#  define PQXX_POTENTIAL_BINARY_ARG typename
#endif


/// Cast binary data to a type that libpqxx will recognise as binary.
/** There are many different formats for storing binary data in memory.  You
 * may have yours as a `std::string`, or a `std::vector<uchar_t>`, or one of
 * many other types.
 *
 * But for libpqxx to recognise your data as binary, it needs to be a
 * `std::basic_string<std::byte>`, or a `std::basic_string_view<std::byte>`;
 * or in C++20 or better, any contiguous block of `std::byte`.
 *
 * Use `binary_cast` as a convenience helper to cast your data as a
 * `std::basic_string_view<std::byte>`.
 *
 * @warning There are two things you should be aware of!  First, the data must
 * be contiguous in memory.  In C++20 the compiler will enforce this, but in
 * C++17 it's your own problem.  Second, you must keep the object where you
 * store the actual data alive for as long as you might use this function's
 * return value.
 */
template<PQXX_POTENTIAL_BINARY_ARG TYPE>
std::basic_string_view<std::byte> binary_cast(TYPE const &data)
{
  static_assert(sizeof(value_type<TYPE>) == 1);
  return {
    reinterpret_cast<std::byte const *>(
      const_cast<strip_t<decltype(*std::data(data))> const *>(
        std::data(data))),
    std::size(data)};
}


#if defined(PQXX_HAVE_CONCEPTS)
template<typename CHAR>
concept char_sized = (sizeof(CHAR) == 1);
#  define PQXX_CHAR_SIZED_ARG char_sized
#else
#  define PQXX_CHAR_SIZED_ARG typename
#endif

/// Construct a type that libpqxx will recognise as binary.
/** Takes a data pointer and a size, without being too strict about their
 * types, and constructs a `std::basic_string_view<std::byte>` pointing to
 * the same data.
 *
 * This makes it a little easier to turn binary data, in whatever form you
 * happen to have it, into binary data as libpqxx understands it.
 */
template<PQXX_CHAR_SIZED_ARG CHAR, typename SIZE>
std::basic_string_view<std::byte> binary_cast(CHAR const *data, SIZE size)
{
  static_assert(sizeof(CHAR) == 1);
  return {
    reinterpret_cast<std::byte const *>(data),
    check_cast<std::size_t>(size, "binary data size")};
}


/// The "null" oid.
constexpr oid oid_none{0};
} // namespace pqxx


/// Private namespace for libpqxx's internal use; do not access.
/** This namespace hides definitions internal to libpqxx.  These are not
 * supposed to be used by client programs, and they may change at any time
 * without notice.
 *
 * Conversely, if you find something in this namespace tremendously useful, by
 * all means do lodge a request for its publication.
 *
 * @warning Here be dragons!
 */
namespace pqxx::internal
{
using namespace std::literals;


/// A safer and more generic replacement for `std::isdigit`.
/** Turns out `std::isdigit` isn't as easy to use as it sounds.  It takes an
 * `int`, but requires it to be nonnegative.  Which means it's an outright
 * liability on systems where `char` is signed.
 */
template<typename CHAR> inline constexpr bool is_digit(CHAR c) noexcept
{
  return (c >= '0') and (c <= '9');
}


/// Describe an object for humans, based on class name and optional name.
/** Interprets an empty name as "no name given."
 */
[[nodiscard]] std::string
describe_object(std::string_view class_name, std::string_view name);


/// Check validity of registering a new "guest" in a "host."
/** The host might be e.g. a connection, and the guest a transaction.  The
 * host can only have one guest at a time, so it is an error to register a new
 * guest while the host already has a guest.
 *
 * If the new registration is an error, this function throws a descriptive
 * exception.
 *
 * Pass the old guest (if any) and the new guest (if any), for both, a type
 * name (at least if the guest is not null), and optionally an object name
 * (but which may be omitted if the caller did not assign one).
 */
void check_unique_register(
  void const *old_guest, std::string_view old_class, std::string_view old_name,
  void const *new_guest, std::string_view new_class,
  std::string_view new_name);


/// Like @ref check_unique_register, but for un-registering a guest.
/** Pass the guest which was registered, as well as the guest which is being
 * unregistered, so that the function can check that they are the same one.
 */
void check_unique_unregister(
  void const *old_guest, std::string_view old_class, std::string_view old_name,
  void const *new_guest, std::string_view new_class,
  std::string_view new_name);


/// Compute buffer size needed to escape binary data for use as a BYTEA.
/** This uses the hex-escaping format.  The return value includes room for the
 * "\x" prefix.
 */
inline constexpr std::size_t size_esc_bin(std::size_t binary_bytes) noexcept
{
  return 2 + (2 * binary_bytes) + 1;
}


/// Compute binary size from the size of its escaped version.
/** Do not include a terminating zero in `escaped_bytes`.
 */
inline constexpr std::size_t size_unesc_bin(std::size_t escaped_bytes) noexcept
{
  return (escaped_bytes - 2) / 2;
}


// TODO: Use actual binary type for "data".
/// Hex-escape binary data into a buffer.
/** The buffer must be able to accommodate
 * `size_esc_bin(std::size(binary_data))` bytes, and the function will write
 * exactly that number of bytes into the buffer.  This includes a trailing
 * zero.
 */
void PQXX_LIBEXPORT
esc_bin(std::basic_string_view<std::byte> binary_data, char buffer[]) noexcept;


/// Hex-escape binary data into a std::string.
std::string PQXX_LIBEXPORT
esc_bin(std::basic_string_view<std::byte> binary_data);


/// Reconstitute binary data from its escaped version.
void PQXX_LIBEXPORT
unesc_bin(std::string_view escaped_data, std::byte buffer[]);


/// Reconstitute binary data from its escaped version.
std::basic_string<std::byte>
  PQXX_LIBEXPORT unesc_bin(std::string_view escaped_data);


/// Transitional: std::ssize(), or custom implementation if not available.
template<typename T> auto ssize(T const &c)
{
#if defined(__cpp_lib_ssize) && __cplusplus >= __cpp_lib_ssize
  return std::ssize(c);
#else
  using signed_t = std::make_signed_t<decltype(std::size(c))>;
  return static_cast<signed_t>(std::size(c));
#endif // __cpp_lib_ssize
}


/// Helper for determining a function's parameter types.
/** This function has no definition.  It's not meant to be actually called.
 * It's just there for pattern-matching in the compiler, so we can use its
 * hypothetical return value.
 */
template<typename RETURN, typename... ARGS>
std::tuple<ARGS...> args_f(RETURN (&func)(ARGS...));


/// Helper for determining a `std::function`'s parameter types.
/** This function has no definition.  It's not meant to be actually called.
 * It's just there for pattern-matching in the compiler, so we can use its
 * hypothetical return value.
 */
template<typename RETURN, typename... ARGS>
std::tuple<ARGS...> args_f(std::function<RETURN(ARGS...)> const &);


/// Helper for determining a member function's parameter types.
/** This function has no definition.  It's not meant to be actually called.
 * It's just there for pattern-matching in the compiler, so we can use its
 * hypothetical return value.
 */
template<typename CLASS, typename RETURN, typename... ARGS>
std::tuple<ARGS...> member_args_f(RETURN (CLASS::*)(ARGS...));


/// Helper for determining a const member function's parameter types.
/** This function has no definition.  It's not meant to be actually called.
 * It's just there for pattern-matching in the compiler, so we can use its
 * hypothetical return value.
 */
template<typename CLASS, typename RETURN, typename... ARGS>
std::tuple<ARGS...> member_args_f(RETURN (CLASS::*)(ARGS...) const);


/// Helper for determining a callable type's parameter types.
/** This specialisation should work for lambdas.
 *
 * This function has no definition.  It's not meant to be actually called.
 * It's just there for pattern-matching in the compiler, so we can use its
 * hypothetical return value.
 */
template<typename CALLABLE>
auto args_f(CALLABLE const &f)
  -> decltype(member_args_f(&CALLABLE::operator()));


/// A callable's parameter types, as a tuple.
template<typename CALLABLE>
using args_t = decltype(args_f(std::declval<CALLABLE>()));


/// Helper: Apply `strip_t` to each of a tuple type's component types.
/** This function has no definition.  It is not meant to be called, only to be
 * used to deduce the right types.
 */
template<typename... TYPES>
std::tuple<strip_t<TYPES>...> strip_types(std::tuple<TYPES...> const &);


/// Take a tuple type and apply @ref strip_t to its component types.
template<typename... TYPES>
using strip_types_t = decltype(strip_types(std::declval<TYPES...>()));
} // namespace pqxx::internal
#endif
