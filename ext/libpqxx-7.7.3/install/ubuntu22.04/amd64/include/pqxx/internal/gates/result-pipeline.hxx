#include <pqxx/internal/callgate.hxx>

namespace pqxx::internal::gate
{
class PQXX_PRIVATE result_pipeline : callgate<result const>
{
  friend class pqxx::pipeline;

  result_pipeline(reference x) : super(x) {}

  std::shared_ptr<std::string const> query_ptr() const
  {
    return home().query_ptr();
  }
};
} // namespace pqxx::internal::gate
