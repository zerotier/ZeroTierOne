#include <pqxx/internal/callgate.hxx>

#include "pqxx/stream_to.hxx"


namespace pqxx::internal::gate
{
class PQXX_PRIVATE connection_stream_to : callgate<connection>
{
  friend class pqxx::stream_to;

  connection_stream_to(reference x) : super(x) {}

  void write_copy_line(std::string_view line) { home().write_copy_line(line); }
  void end_copy_write() { home().end_copy_write(); }
};
} // namespace pqxx::internal::gate
