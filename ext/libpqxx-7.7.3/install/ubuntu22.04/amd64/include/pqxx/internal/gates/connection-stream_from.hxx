#include <pqxx/internal/callgate.hxx>

#include "pqxx/connection.hxx"

namespace pqxx::internal::gate
{
class PQXX_PRIVATE connection_stream_from : callgate<connection>
{
  friend class pqxx::stream_from;

  connection_stream_from(reference x) : super{x} {}

  auto read_copy_line() { return home().read_copy_line(); }
};
} // namespace pqxx::internal::gate
