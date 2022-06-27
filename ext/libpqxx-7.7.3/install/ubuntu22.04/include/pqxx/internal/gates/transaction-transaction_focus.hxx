#include <pqxx/internal/callgate.hxx>

#include "pqxx/transaction_base.hxx"

namespace pqxx::internal::gate
{
class PQXX_PRIVATE transaction_transaction_focus : callgate<transaction_base>
{
  friend class pqxx::transaction_focus;

  transaction_transaction_focus(reference x) : super(x) {}

  void register_focus(transaction_focus *focus)
  {
    home().register_focus(focus);
  }
  void unregister_focus(transaction_focus *focus) noexcept
  {
    home().unregister_focus(focus);
  }
  void register_pending_error(zview error)
  {
    home().register_pending_error(error);
  }
  void register_pending_error(std::string &&error)
  {
    home().register_pending_error(std::move(error));
  }
};
} // namespace pqxx::internal::gate
