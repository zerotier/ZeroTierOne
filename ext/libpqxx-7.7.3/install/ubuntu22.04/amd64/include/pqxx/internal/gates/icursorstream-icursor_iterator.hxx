#include <pqxx/internal/callgate.hxx>

namespace pqxx::internal::gate
{
class PQXX_PRIVATE icursorstream_icursor_iterator : callgate<icursorstream>
{
  friend class pqxx::icursor_iterator;

  icursorstream_icursor_iterator(reference x) : super(x) {}

  void insert_iterator(icursor_iterator *i) noexcept
  {
    home().insert_iterator(i);
  }

  void remove_iterator(icursor_iterator *i) const noexcept
  {
    home().remove_iterator(i);
  }

  icursorstream::size_type forward() { return home().forward(); }
  icursorstream::size_type forward(icursorstream::size_type n)
  {
    return home().forward(n);
  }

  void service_iterators(icursorstream::difference_type p)
  {
    home().service_iterators(p);
  }
};
} // namespace pqxx::internal::gate
