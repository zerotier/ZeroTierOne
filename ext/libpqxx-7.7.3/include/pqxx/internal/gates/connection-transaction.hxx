#include <pqxx/internal/callgate.hxx>

namespace pqxx
{
class connection;
}

namespace pqxx::internal::gate
{
class PQXX_PRIVATE connection_transaction : callgate<connection>
{
  friend class pqxx::transaction_base;

  connection_transaction(reference x) : super(x) {}

  template<typename STRING> result exec(STRING query, std::string_view desc)
  {
    return home().exec(query, desc);
  }

  void register_transaction(transaction_base *t)
  {
    home().register_transaction(t);
  }
  void unregister_transaction(transaction_base *t) noexcept
  {
    home().unregister_transaction(t);
  }

  auto read_copy_line() { return home().read_copy_line(); }
  void write_copy_line(std::string_view line) { home().write_copy_line(line); }
  void end_copy_write() { home().end_copy_write(); }

  result exec_prepared(zview statement, internal::c_params const &args)
  {
    return home().exec_prepared(statement, args);
  }

  result exec_params(zview query, internal::c_params const &args)
  {
    return home().exec_params(query, args);
  }
};
} // namespace pqxx::internal::gate
