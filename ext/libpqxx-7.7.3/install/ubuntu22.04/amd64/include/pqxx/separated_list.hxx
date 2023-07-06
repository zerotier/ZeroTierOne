/* Helper similar to Python's `str.join()`.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/separated_list instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_SEPARATED_LIST
#define PQXX_H_SEPARATED_LIST

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <algorithm>
#include <numeric>

#include "pqxx/strconv.hxx"

// C++20: Simplify using std::ranges::range.
// C++20: Optimise buffer allocation using random_access_range/iterator.
namespace pqxx
{
/**
 * @defgroup utility Utility functions
 */
//@{

/// Represent sequence of values as a string, joined by a given separator.
/**
 * Use this to turn e.g. the numbers 1, 2, and 3 into a string "1, 2, 3".
 *
 * @param sep separator string (to be placed between items)
 * @param begin beginning of items sequence
 * @param end end of items sequence
 * @param access functor defining how to dereference sequence elements
 */
template<typename ITER, typename ACCESS>
[[nodiscard]] inline std::string
separated_list(std::string_view sep, ITER begin, ITER end, ACCESS access)
{
  if (end == begin)
    return {};
  auto next{begin};
  ++next;
  if (next == end)
    return to_string(access(begin));

  // From here on, we've got at least 2 elements -- meaning that we need sep.
  using elt_type = strip_t<decltype(access(begin))>;
  using traits = string_traits<elt_type>;

  std::size_t budget{0};
  for (ITER cnt{begin}; cnt != end; ++cnt)
    budget += traits::size_buffer(access(cnt));
  budget +=
    static_cast<std::size_t>(std::distance(begin, end)) * std::size(sep);

  std::string result;
  result.resize(budget);

  char *const data{result.data()};
  char *here{data};
  char *stop{data + budget};
  here = traits::into_buf(here, stop, access(begin)) - 1;
  for (++begin; begin != end; ++begin)
  {
    here += sep.copy(here, std::size(sep));
    here = traits::into_buf(here, stop, access(begin)) - 1;
  }
  result.resize(static_cast<std::size_t>(here - data));
  return result;
}


/// Render sequence as a string, using given separator between items.
template<typename ITER>
[[nodiscard]] inline std::string
separated_list(std::string_view sep, ITER begin, ITER end)
{
  return separated_list(sep, begin, end, [](ITER i) { return *i; });
}


/// Render items in a container as a string, using given separator.
template<typename CONTAINER>
[[nodiscard]] inline auto
separated_list(std::string_view sep, CONTAINER const &c)
  /*
  Always std::string; necessary because SFINAE doesn't work with the
  contents of function bodies, so the check for iterability has to be in
  the signature.
  */
  -> typename std::enable_if<
    (not std::is_void<decltype(std::begin(c))>::value and
     not std::is_void<decltype(std::end(c))>::value),
    std::string>::type
{
  return separated_list(sep, std::begin(c), std::end(c));
}


/// Render items in a tuple as a string, using given separator.
template<
  typename TUPLE, std::size_t INDEX = 0, typename ACCESS,
  typename std::enable_if<
    (INDEX == std::tuple_size<TUPLE>::value - 1), int>::type = 0>
[[nodiscard]] inline std::string separated_list(
  std::string_view /* sep */, TUPLE const &t, ACCESS const &access)
{
  return to_string(access(&std::get<INDEX>(t)));
}

template<
  typename TUPLE, std::size_t INDEX = 0, typename ACCESS,
  typename std::enable_if<
    (INDEX < std::tuple_size<TUPLE>::value - 1), int>::type = 0>
[[nodiscard]] inline std::string
separated_list(std::string_view sep, TUPLE const &t, ACCESS const &access)
{
  std::string out{to_string(access(&std::get<INDEX>(t)))};
  out.append(sep);
  out.append(separated_list<TUPLE, INDEX + 1>(sep, t, access));
  return out;
}

template<
  typename TUPLE, std::size_t INDEX = 0,
  typename std::enable_if<
    (INDEX <= std::tuple_size<TUPLE>::value), int>::type = 0>
[[nodiscard]] inline std::string
separated_list(std::string_view sep, TUPLE const &t)
{
  // TODO: Optimise allocation.
  return separated_list(sep, t, [](TUPLE const &tup) { return *tup; });
}
//@}
} // namespace pqxx
#endif
