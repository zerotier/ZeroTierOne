#include <pqxx/internal/callgate.hxx>

namespace pqxx::internal::gate
{
class PQXX_PRIVATE result_connection : callgate<result const>
{
  friend class pqxx::connection;

  result_connection(reference x) : super(x) {}

  operator bool() const { return bool(home()); }
  bool operator!() const { return not home(); }
};
} // namespace pqxx::internal::gate
