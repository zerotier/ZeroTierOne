#include <pqxx/internal/callgate.hxx>

namespace pqxx::internal::gate
{
class PQXX_PRIVATE transaction_sql_cursor : callgate<transaction_base>
{
  friend class pqxx::internal::sql_cursor;
  transaction_sql_cursor(reference x) : super(x) {}
};
} // namespace pqxx::internal::gate
