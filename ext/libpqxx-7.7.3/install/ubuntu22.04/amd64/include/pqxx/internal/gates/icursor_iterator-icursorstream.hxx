#include <pqxx/internal/callgate.hxx>

namespace pqxx::internal::gate
{
class PQXX_PRIVATE icursor_iterator_icursorstream : callgate<icursor_iterator>
{
  friend class pqxx::icursorstream;

  icursor_iterator_icursorstream(reference x) : super(x) {}

  icursor_iterator::difference_type pos() const noexcept
  {
    return home().pos();
  }

  icursor_iterator *get_prev() { return home().m_prev; }
  void set_prev(icursor_iterator *i) { home().m_prev = i; }

  icursor_iterator *get_next() { return home().m_next; }
  void set_next(icursor_iterator *i) { home().m_next = i; }

  void fill(result const &r) { home().fill(r); }
};
} // namespace pqxx::internal::gate
