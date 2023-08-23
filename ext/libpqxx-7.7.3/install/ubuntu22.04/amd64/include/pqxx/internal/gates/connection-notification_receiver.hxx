#include <pqxx/internal/callgate.hxx>

#include "pqxx/connection.hxx"


namespace pqxx
{
class notification_receiver;
}


namespace pqxx::internal::gate
{
class PQXX_PRIVATE connection_notification_receiver : callgate<connection>
{
  friend class pqxx::notification_receiver;

  connection_notification_receiver(reference x) : super(x) {}

  void add_receiver(notification_receiver *receiver)
  {
    home().add_receiver(receiver);
  }
  void remove_receiver(notification_receiver *receiver) noexcept
  {
    home().remove_receiver(receiver);
  }
};
} // namespace pqxx::internal::gate
