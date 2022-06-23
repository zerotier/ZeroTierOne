/** Functions that wait.
 */
#include "pqxx-source.hxx"

// The <thread> header is still broken on MinGW.  :-(
#if defined(PQXX_HAVE_SLEEP_FOR)
#  include <thread>
#endif

// For WSAPoll():
#if __has_include(<winsock2.h>)
#  include <winsock2.h>
#  define PQXX_HAVE_SELECT
#endif
#if __has_include(<ws2tcpip.h>)
#  include <ws2tcpip.h>
#endif
#if __has_include(<mstcpip.h>)
#  include <mstcpip.h>
#endif

// For poll():
#if __has_include(<poll.h>)
#  include <poll.h>
#endif

// For select() on recent POSIX systems.
#if __has_include(<sys/select.h>)
#  include <sys/select.h>
#  define PQXX_HAVE_SELECT
#endif

// For select() on some older POSIX systems.
#if __has_include(<sys/types.h>)
#  include <sys/types.h>
#  define PQXX_HAVE_SELECT
#endif
#if __has_include(<unistd.h>)
#  include <unistd.h>
#endif
#if __has_include(<sys/time.h>)
#  include <sys/time.h>
#endif


#include "pqxx/internal/header-pre.hxx"

#include "pqxx/internal/wait.hxx"
#include "pqxx/util.hxx"

#include "pqxx/internal/header-post.hxx"


namespace
{
template<typename T> T to_milli(unsigned seconds, unsigned microseconds)
{
  return pqxx::check_cast<T>(
    (seconds * 1000) + (microseconds / 1000),
    "Wait timeout value out of bounds.");
}


#if defined(PQXX_HAVE_SELECT)
/// Set a bit on an fd_set.
[[maybe_unused]] void set_fdbit(fd_set &bits, int fd)
{
#  ifdef _MSC_VER
// Suppress pointless, unfixable warnings in Visual Studio.
#    pragma warning(push)
#    pragma warning(disable : 4389) // Signed/unsigned mismatch.
#    pragma warning(disable : 4127) // Conditional expression is constant.
#  endif
  FD_SET(fd, &bits);
#  ifdef _MSV_VER
// Restore prevalent warning settings.
#    pragma warning(pop)
#  endif
}
#endif
} // namespace


void pqxx::internal::wait_fd(
  int fd, bool for_read, bool for_write, unsigned seconds,
  unsigned microseconds)
{
// WSAPoll is available in winsock2.h only for versions of Windows >= 0x0600
#if defined(_WIN32) && (_WIN32_WINNT >= 0x0600)
  short const events{static_cast<short>(
    (for_read ? POLLRDNORM : 0) | (for_write ? POLLWRNORM : 0))};
  WSAPOLLFD fdarray{SOCKET(fd), events, 0};
  WSAPoll(&fdarray, 1u, to_milli<unsigned>(seconds, microseconds));
  // TODO: Check for errors.
#elif defined(PQXX_HAVE_POLL)
  auto const events{static_cast<short>(
    POLLERR | POLLHUP | POLLNVAL | (for_read ? POLLIN : 0) |
    (for_write ? POLLOUT : 0))};
  pollfd pfd{fd, events, 0};
  poll(&pfd, 1, to_milli<int>(seconds, microseconds));
  // TODO: Check for errors.
#else
  // No poll()?  Our last option is select().
  fd_set read_fds;
  FD_ZERO(&read_fds);
  if (for_read)
    set_fdbit(read_fds, fd);

  fd_set write_fds;
  FD_ZERO(&write_fds);
  if (for_write)
    set_fdbit(write_fds, fd);

  fd_set except_fds;
  FD_ZERO(&except_fds);
  set_fdbit(except_fds, fd);

  timeval tv = {seconds, microseconds};
  select(fd + 1, &read_fds, &write_fds, &except_fds, &tv);
  // TODO: Check for errors.
#endif
}


void PQXX_COLD pqxx::internal::wait_for(unsigned int microseconds)
{
#if defined(PQXX_HAVE_SLEEP_FOR)
  std::this_thread::sleep_for(std::chrono::microseconds{microseconds});
#else
  // MinGW still does not have a functioning <thread> header.  Work around this
  // using select().
  // Not worth optimising for though -- they'll have to fix it at some point.
  timeval tv{microseconds / 1'000'000u, microseconds % 1'000'000u};
  select(0, nullptr, nullptr, nullptr, &tv);
#endif
}
