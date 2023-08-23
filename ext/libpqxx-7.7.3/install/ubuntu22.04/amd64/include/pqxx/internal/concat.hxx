#if !defined(PQXX_CONCAT_HXX)
#  define PQXX_CONCAT_HXX

#  include <string>
#  include <string_view>

#  include "pqxx/strconv.hxx"

namespace pqxx::internal
{
/// Convert item to a string, write it into [here, end).
template<typename TYPE>
void render_item(TYPE const &item, char *&here, char *end)
{
  here = string_traits<TYPE>::into_buf(here, end, item) - 1;
}


// C++20: Support non-random_access_range ranges.
/// Efficiently combine a bunch of items into one big string.
/** Use this as an optimised version of string concatentation.  It takes just
 * about any type; it will represent each item as a string according to its
 * @ref string_traits.
 *
 * This is a simpler, more specialised version of @ref separated_list for a
 * statically known series of items, possibly of different types.
 */
template<typename... TYPE>
[[nodiscard]] inline std::string concat(TYPE... item)
{
  std::string buf;
  // Size to accommodate string representations of all inputs, minus their
  // terminating zero bytes.
  buf.resize(size_buffer(item...));

  char *const data{buf.data()};
  char *here = data;
  char *end = data + std::size(buf);
  (render_item(item, here, end), ...);

  buf.resize(static_cast<std::size_t>(here - data));
  return buf;
}
} // namespace pqxx::internal
#endif
