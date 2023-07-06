#include "pqxx/internal/libpq-forward.hxx"
#include <pqxx/internal/callgate.hxx>

#include "pqxx/pipeline.hxx"

namespace pqxx::internal::gate
{
class PQXX_PRIVATE connection_pipeline : callgate<connection>
{
  friend class pqxx::pipeline;

  connection_pipeline(reference x) : super(x) {}

  void start_exec(char const query[]) { home().start_exec(query); }
  pqxx::internal::pq::PGresult *get_result() { return home().get_result(); }
  void cancel_query() { home().cancel_query(); }

  bool consume_input() noexcept { return home().consume_input(); }
  bool is_busy() const noexcept { return home().is_busy(); }

  int encoding_id() { return home().encoding_id(); }
};
} // namespace pqxx::internal::gate
