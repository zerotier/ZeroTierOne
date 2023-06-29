/** Result loops.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_RESULT_ITER
#define PQXX_H_RESULT_ITER

#include <memory>

#include "pqxx/strconv.hxx"

namespace pqxx
{
class result;
} // namespace pqxx


namespace pqxx::internal
{
// C++20: Replace with generator?
/// Iterator for looped unpacking of a result.
template<typename... TYPE> class result_iter
{
public:
  using value_type = std::tuple<TYPE...>;

  /// Construct an "end" iterator.
  result_iter() = default;

  explicit result_iter(result const &home) :
          m_home{&home}, m_size{std::size(home)}
  {
    if (not std::empty(home))
      read();
  }
  result_iter(result_iter const &) = default;

  result_iter &operator++()
  {
    m_index++;
    if (m_index >= m_size)
      m_home = nullptr;
    else
      read();
    return *this;
  }

  /// Comparison only works for comparing to end().
  bool operator==(result_iter const &rhs) const
  {
    return m_home == rhs.m_home;
  }
  bool operator!=(result_iter const &rhs) const { return not(*this == rhs); }

  value_type const &operator*() const { return m_value; }

private:
  void read() { (*m_home)[m_index].convert(m_value); }

  result const *m_home{nullptr};
  result::size_type m_index{0};
  result::size_type m_size;
  value_type m_value;
};


template<typename... TYPE> class result_iteration
{
public:
  using iterator = result_iter<TYPE...>;
  explicit result_iteration(result const &home) : m_home{home}
  {
    constexpr auto tup_size{sizeof...(TYPE)};
    if (home.columns() != tup_size)
      throw usage_error{internal::concat(
        "Tried to extract ", to_string(tup_size),
        " field(s) from a result with ", to_string(home.columns()),
        " column(s).")};
  }
  iterator begin() const
  {
    if (std::size(m_home) == 0)
      return end();
    else
      return iterator{m_home};
  }
  iterator end() const { return {}; }

private:
  pqxx::result const &m_home;
};
} // namespace pqxx::internal


template<typename... TYPE> inline auto pqxx::result::iter() const
{
  return pqxx::internal::result_iteration<TYPE...>{*this};
}


template<typename CALLABLE>
inline void pqxx::result::for_each(CALLABLE &&func) const
{
  using args_tuple = internal::args_t<decltype(func)>;
  constexpr auto sz{std::tuple_size_v<args_tuple>};
  static_assert(
    sz > 0,
    "Callback for for_each must take parameters, one for each column in the "
    "result.");

  auto const cols{this->columns()};
  if (sz != cols)
    throw usage_error{internal::concat(
      "Callback to for_each takes ", sz, "parameter", (sz == 1) ? "" : "s",
      ", but result set has ", cols, "field", (cols == 1) ? "" : "s", ".")};

  using pass_tuple = pqxx::internal::strip_types_t<args_tuple>;
  for (auto const r : *this) std::apply(func, r.as_tuple<pass_tuple>());
}
#endif
