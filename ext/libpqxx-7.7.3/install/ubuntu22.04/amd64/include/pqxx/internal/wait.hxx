#if !defined(PQXX_WAIT_HXX)
#  define PQXX_WAIT_HXX

namespace pqxx::internal
{
/// Wait.
/** This is normally `std::this_thread::sleep_for()`.  But MinGW's `thread`
 * header doesn't work, so we must be careful about including it.
 */
void PQXX_LIBEXPORT wait_for(unsigned int microseconds);


/// Wait for a socket to be ready for reading/writing, or timeout.
PQXX_LIBEXPORT void wait_fd(
  int fd, bool for_read, bool for_write, unsigned seconds = 1,
  unsigned microseconds = 0);
} // namespace pqxx::internal
#endif
