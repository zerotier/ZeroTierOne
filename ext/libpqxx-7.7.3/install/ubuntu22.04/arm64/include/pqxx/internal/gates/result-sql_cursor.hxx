#include <pqxx/internal/callgate.hxx>

namespace pqxx::internal::gate
{
class PQXX_PRIVATE result_sql_cursor : callgate<result const>
{
  friend class pqxx::internal::sql_cursor;

  result_sql_cursor(reference x) : super(x) {}

  char const *cmd_status() const noexcept { return home().cmd_status(); }
};
} // namespace pqxx::internal::gate
