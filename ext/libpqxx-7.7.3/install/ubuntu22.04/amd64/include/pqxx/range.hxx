#ifndef PQXX_H_RANGE
#define PQXX_H_RANGE

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <variant>

#include "pqxx/internal/array-composite.hxx"
#include "pqxx/internal/concat.hxx"

namespace pqxx
{
/// An _unlimited_ boundary value to a @ref pqxx::range.
/** Use this as a lower or upper bound for a range if the range should extend
 * to infinity on that side.
 *
 * An unlimited boundary is always inclusive of "infinity" values, if the
 * range's value type supports them.
 */
struct no_bound
{
  template<typename TYPE> constexpr bool extends_down_to(TYPE const &) const
  {
    return true;
  }
  template<typename TYPE> constexpr bool extends_up_to(TYPE const &) const
  {
    return true;
  }
};


/// An _inclusive_ boundary value to a @ref pqxx::range.
/** Use this as a lower or upper bound for a range if the range should include
 * the value.
 */
template<typename TYPE> class inclusive_bound
{
public:
  inclusive_bound() = delete;
  explicit inclusive_bound(TYPE const &value) : m_value{value}
  {
    if (is_null(value))
      throw argument_error{"Got null value as an inclusive range bound."};
  }

  [[nodiscard]] constexpr TYPE const &get() const &noexcept { return m_value; }

  // TODO: constexpr and/or noexcept if underlying operator supports it.
  /// Would this bound, as a lower bound, include value?
  [[nodiscard]] bool extends_down_to(TYPE const &value) const
  {
    return not(value < m_value);
  }

  // TODO: constexpr and/or noexcept if underlying operator supports it.
  /// Would this bound, as an upper bound, include value?
  [[nodiscard]] bool extends_up_to(TYPE const &value) const
  {
    return not(m_value < value);
  }

private:
  TYPE m_value;
};


/// An _exclusive_ boundary value to a @ref pqxx::range.
/** Use this as a lower or upper bound for a range if the range should _not_
 * include the value.
 */
template<typename TYPE> class exclusive_bound
{
public:
  exclusive_bound() = delete;
  explicit exclusive_bound(TYPE const &value) : m_value{value}
  {
    if (is_null(value))
      throw argument_error{"Got null value as an exclusive range bound."};
  }

  [[nodiscard]] constexpr TYPE const &get() const &noexcept { return m_value; }

  // TODO: constexpr and/or noexcept if underlying operator supports it.
  /// Would this bound, as a lower bound, include value?
  [[nodiscard]] bool extends_down_to(TYPE const &value) const
  {
    return m_value < value;
  }

  // TODO: constexpr and/or noexcept if underlying operator supports it.
  /// Would this bound, as an upper bound, include value?
  [[nodiscard]] bool extends_up_to(TYPE const &value) const
  {
    return value < m_value;
  }

private:
  TYPE m_value;
};


/// A range boundary value.
/** A range bound is either no bound at all; or an inclusive bound; or an
 * exclusive bound.  Pass one of the three to the constructor.
 */
template<typename TYPE> class range_bound
{
public:
  range_bound() = delete;
  // TODO: constexpr and/or noexcept if underlying constructor supports it.
  range_bound(no_bound) : m_bound{} {}
  // TODO: constexpr and/or noexcept if underlying constructor supports it.
  range_bound(inclusive_bound<TYPE> const &bound) : m_bound{bound} {}
  // TODO: constexpr and/or noexcept if underlying constructor supports it.
  range_bound(exclusive_bound<TYPE> const &bound) : m_bound{bound} {}
  // TODO: constexpr and/or noexcept if underlying constructor supports it.
  range_bound(range_bound const &) = default;
  // TODO: constexpr and/or noexcept if underlying constructor supports it.
  range_bound(range_bound &&) = default;

  // TODO: constexpr and/or noexcept if underlying operators support it.
  bool operator==(range_bound const &rhs) const
  {
    if (this->is_limited())
      return (
        rhs.is_limited() and (this->is_inclusive() == rhs.is_inclusive()) and
        (*this->value() == *rhs.value()));
    else
      return not rhs.is_limited();
  }

  // TODO: constexpr and/or noexcept if underlying operator supports it.
  bool operator!=(range_bound const &rhs) const { return not(*this == rhs); }
  range_bound &operator=(range_bound const &) = default;
  range_bound &operator=(range_bound &&) = default;

  /// Is this a finite bound?
  constexpr bool is_limited() const noexcept
  {
    return not std::holds_alternative<no_bound>(m_bound);
  }

  /// Is this boundary an inclusive one?
  constexpr bool is_inclusive() const noexcept
  {
    return std::holds_alternative<inclusive_bound<TYPE>>(m_bound);
  }

  /// Is this boundary an exclusive one?
  constexpr bool is_exclusive() const noexcept
  {
    return std::holds_alternative<exclusive_bound<TYPE>>(m_bound);
  }

  // TODO: constexpr/noexcept if underlying function supports it.
  /// Would this bound, as a lower bound, include `value`?
  bool extends_down_to(TYPE const &value) const
  {
    return std::visit(
      [&value](auto const &bound) { return bound.extends_down_to(value); },
      m_bound);
  }

  // TODO: constexpr/noexcept if underlying function supports it.
  /// Would this bound, as an upper bound, include `value`?
  bool extends_up_to(TYPE const &value) const
  {
    return std::visit(
      [&value](auto const &bound) { return bound.extends_up_to(value); },
      m_bound);
  }

  /// Return bound value, or `nullptr` if it's not limited.
  [[nodiscard]] constexpr TYPE const *value() const &noexcept
  {
    return std::visit(
      [](auto const &bound) noexcept {
        using bound_t = std::decay_t<decltype(bound)>;
        if constexpr (std::is_same_v<bound_t, no_bound>)
          return static_cast<TYPE const *>(nullptr);
        else
          return &bound.get();
      },
      m_bound);
  }

private:
  std::variant<no_bound, inclusive_bound<TYPE>, exclusive_bound<TYPE>> m_bound;
};


// C++20: Concepts for comparisons, construction, etc.
/// A C++ equivalent to PostgreSQL's range types.
/** You can use this as a client-side representation of a "range" in SQL.
 *
 * PostgreSQL defines several range types, differing in the data type over
 * which they range.  You can also define your own range types.
 *
 * Usually you'll want the server to deal with ranges.  But on occasions where
 * you need to work with them client-side, you may want to use @ref
 * pqxx::range. (In cases where all you do is pass them along to the server
 * though, it's not worth the complexity.  In that case you might as well treat
 * ranges as just strings.)
 *
 * For documentation on PostgreSQL's range types, see:
 * https://www.postgresql.org/docs/current/rangetypes.html
 *
 * The value type must be copyable and default-constructible, and support the
 * less-than (`<`) and equals (`==`) comparisons.  Value initialisation must
 * produce a consistent value.
 */
template<typename TYPE> class range
{
public:
  /// Create a range.
  /** For each of the two bounds, pass a @ref no_bound, @ref inclusive_bound,
   * or
   * @ref exclusive_bound.
   */
  range(range_bound<TYPE> lower, range_bound<TYPE> upper) :
          m_lower{lower}, m_upper{upper}
  {
    if (
      lower.is_limited() and upper.is_limited() and
      (*upper.value() < *lower.value()))
      throw range_error{internal::concat(
        "Range's lower bound (", *lower.value(),
        ") is greater than its upper bound (", *upper.value(), ").")};
  }

  // TODO: constexpr and/or noexcept if underlying constructor supports it.
  /// Create an empty range.
  /** SQL has a separate literal to denote an empty range, but any range which
   * encompasses no values is an empty range.
   */
  range() :
          m_lower{exclusive_bound<TYPE>{TYPE{}}},
          m_upper{exclusive_bound<TYPE>{TYPE{}}}
  {}

  // TODO: constexpr and/or noexcept if underlying operators support it.
  bool operator==(range const &rhs) const
  {
    return (this->lower_bound() == rhs.lower_bound() and
            this->upper_bound() == rhs.upper_bound()) or
           (this->empty() and rhs.empty());
  }

  // TODO: constexpr and/or noexcept if underlying operator supports it.
  bool operator!=(range const &rhs) const { return !(*this == rhs); }

  range(range const &) = default;
  range(range &&) = default;
  range &operator=(range const &) = default;
  range &operator=(range &&) = default;

  // TODO: constexpr and/or noexcept if underlying operator supports it.
  /// Is this range clearly empty?
  /** An empty range encompasses no values.
   *
   * It is possible to "fool" this.  For example, if your range is of an
   * integer type and has exclusive bounds of 0 and 1, it encompasses no values
   * but its `empty()` will return false.  The PostgreSQL implementation, by
   * contrast, will notice that it is empty.  Similar things can happen for
   * floating-point types, but with more subtleties and edge cases.
   */
  bool empty() const
  {
    return (m_lower.is_exclusive() or m_upper.is_exclusive()) and
           m_lower.is_limited() and m_upper.is_limited() and
           not(*m_lower.value() < *m_upper.value());
  }

  // TODO: constexpr and/or noexcept if underlying functions support it.
  /// Does this range encompass `value`?
  bool contains(TYPE value) const
  {
    return m_lower.extends_down_to(value) and m_upper.extends_up_to(value);
  }

  // TODO: constexpr and/or noexcept if underlying operators support it.
  /// Does this range encompass all of `other`?
  /** This function is not particularly smart.  It does not know, for example,
   * that integer ranges `[0,9]` and `[0,10)` contain the same values.
   */
  bool contains(range<TYPE> const &other) const
  {
    return (*this & other) == other;
  }

  [[nodiscard]] constexpr range_bound<TYPE> const &
  lower_bound() const &noexcept
  {
    return m_lower;
  }
  [[nodiscard]] constexpr range_bound<TYPE> const &
  upper_bound() const &noexcept
  {
    return m_upper;
  }

  // TODO: constexpr and/or noexcept if underlying operators support it.
  /// Intersection of two ranges.
  /** Returns a range describing those values which are in both ranges.
   */
  range operator&(range const &other) const
  {
    range_bound<TYPE> lower{no_bound{}};
    if (not this->lower_bound().is_limited())
      lower = other.lower_bound();
    else if (not other.lower_bound().is_limited())
      lower = this->lower_bound();
    else if (*this->lower_bound().value() < *other.lower_bound().value())
      lower = other.lower_bound();
    else if (*other.lower_bound().value() < *this->lower_bound().value())
      lower = this->lower_bound();
    else if (this->lower_bound().is_exclusive())
      lower = this->lower_bound();
    else
      lower = other.lower_bound();

    range_bound<TYPE> upper{no_bound{}};
    if (not this->upper_bound().is_limited())
      upper = other.upper_bound();
    else if (not other.upper_bound().is_limited())
      upper = this->upper_bound();
    else if (*other.upper_bound().value() < *this->upper_bound().value())
      upper = other.upper_bound();
    else if (*this->upper_bound().value() < *other.upper_bound().value())
      upper = this->upper_bound();
    else if (this->upper_bound().is_exclusive())
      upper = this->upper_bound();
    else
      upper = other.upper_bound();

    if (
      lower.is_limited() and upper.is_limited() and
      (*upper.value() < *lower.value()))
      return {};
    else
      return {lower, upper};
  }

  /// Convert to another base type.
  template<typename DEST> operator range<DEST>() const
  {
    range_bound<DEST> lower{no_bound{}}, upper{no_bound{}};
    if (lower_bound().is_inclusive())
      lower = inclusive_bound<DEST>{*lower_bound().value()};
    else if (lower_bound().is_exclusive())
      lower = exclusive_bound<DEST>{*lower_bound().value()};

    if (upper_bound().is_inclusive())
      upper = inclusive_bound<DEST>{*upper_bound().value()};
    else if (upper_bound().is_exclusive())
      upper = exclusive_bound<DEST>{*upper_bound().value()};

    return {lower, upper};
  }

private:
  range_bound<TYPE> m_lower, m_upper;
};


/// String conversions for a @ref range type.
/** Conversion assumes that either your client encoding is UTF-8, or the values
 * are pure ASCII.
 */
template<typename TYPE> struct string_traits<range<TYPE>>
{
  [[nodiscard]] static inline zview
  to_buf(char *begin, char *end, range<TYPE> const &value)
  {
    return generic_to_buf(begin, end, value);
  }

  static inline char *
  into_buf(char *begin, char *end, range<TYPE> const &value)
  {
    if (value.empty())
    {
      if ((end - begin) <= internal::ssize(s_empty))
        throw conversion_overrun{s_overrun.c_str()};
      char *here = begin + s_empty.copy(begin, std::size(s_empty));
      *here++ = '\0';
      return here;
    }
    else
    {
      if (end - begin < 4)
        throw conversion_overrun{s_overrun.c_str()};
      char *here = begin;
      *here++ =
        (static_cast<char>(value.lower_bound().is_inclusive() ? '[' : '('));
      TYPE const *lower{value.lower_bound().value()};
      // Convert bound (but go back to overwrite that trailing zero).
      if (lower != nullptr)
        here = string_traits<TYPE>::into_buf(here, end, *lower) - 1;
      *here++ = ',';
      TYPE const *upper{value.upper_bound().value()};
      // Convert bound (but go back to overwrite that trailing zero).
      if (upper != nullptr)
        here = string_traits<TYPE>::into_buf(here, end, *upper) - 1;
      if ((end - here) < 2)
        throw conversion_overrun{s_overrun.c_str()};
      *here++ =
        static_cast<char>(value.upper_bound().is_inclusive() ? ']' : ')');
      *here++ = '\0';
      return here;
    }
  }

  [[nodiscard]] static inline range<TYPE> from_string(std::string_view text)
  {
    if (std::size(text) < 3)
      throw pqxx::conversion_error{err_bad_input(text)};
    bool left_inc{false};
    switch (text[0])
    {
    case '[': left_inc = true; break;

    case '(': break;

    case 'e':
    case 'E':
      if (
        (std::size(text) != std::size(s_empty)) or
        (text[1] != 'm' and text[1] != 'M') or
        (text[2] != 'p' and text[2] != 'P') or
        (text[3] != 't' and text[3] != 'T') or
        (text[4] != 'y' and text[4] != 'Y'))
        throw pqxx::conversion_error{err_bad_input(text)};
      return {};
      break;

    default: throw pqxx::conversion_error{err_bad_input(text)};
    }

    auto scan{internal::get_glyph_scanner(internal::encoding_group::UTF8)};
    // The field parser uses this to track which field it's parsing, and
    // when not to expect a field separator.
    std::size_t index{0};
    // The last field we expect to see.
    static constexpr std::size_t last{1};
    // Current parsing position.  We skip the opening parenthesis or bracket.
    std::size_t pos{1};
    // The string may leave out either bound to indicate that it's unlimited.
    std::optional<TYPE> lower, upper;
    // We reuse the same field parser we use for composite values and arrays.
    internal::parse_composite_field(index, text, pos, lower, scan, last);
    internal::parse_composite_field(index, text, pos, upper, scan, last);

    // We need one more character: the closing parenthesis or bracket.
    if (pos != std::size(text))
      throw pqxx::conversion_error{err_bad_input(text)};
    char const closing{text[pos - 1]};
    if (closing != ')' and closing != ']')
      throw pqxx::conversion_error{err_bad_input(text)};
    bool const right_inc{closing == ']'};

    range_bound<TYPE> lower_bound{no_bound{}}, upper_bound{no_bound{}};
    if (lower)
    {
      if (left_inc)
        lower_bound = inclusive_bound{*lower};
      else
        lower_bound = exclusive_bound{*lower};
    }
    if (upper)
    {
      if (right_inc)
        upper_bound = inclusive_bound{*upper};
      else
        upper_bound = exclusive_bound{*upper};
    }

    return {lower_bound, upper_bound};
  }

  [[nodiscard]] static inline constexpr std::size_t
  size_buffer(range<TYPE> const &value) noexcept
  {
    TYPE const *lower{value.lower_bound().value()},
      *upper{value.upper_bound().value()};
    std::size_t const lsz{
      lower == nullptr ? 0 : string_traits<TYPE>::size_buffer(*lower) - 1},
      usz{upper == nullptr ? 0 : string_traits<TYPE>::size_buffer(*upper) - 1};

    if (value.empty())
      return std::size(s_empty) + 1;
    else
      return 1 + lsz + 1 + usz + 2;
  }

private:
  static constexpr zview s_empty{"empty"_zv};
  static constexpr auto s_overrun{"Not enough space in buffer for range."_zv};

  /// Compose error message for invalid range input.
  static std::string err_bad_input(std::string_view text)
  {
    return internal::concat("Invalid range input: '", text, "'");
  }
};


/// A range type does not have an innate null value.
template<typename TYPE> struct nullness<range<TYPE>> : no_null<range<TYPE>>
{};
} // namespace pqxx
#endif
