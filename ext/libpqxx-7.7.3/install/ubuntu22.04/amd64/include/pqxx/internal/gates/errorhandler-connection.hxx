#include <pqxx/internal/callgate.hxx>

namespace pqxx::internal::gate
{
class PQXX_PRIVATE errorhandler_connection : callgate<errorhandler>
{
  friend class pqxx::connection;

  errorhandler_connection(reference x) : super(x) {}

  void unregister() noexcept { home().unregister(); }
};
} // namespace pqxx::internal::gate
