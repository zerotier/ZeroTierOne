// Check for strerror_r.
// It can be either the POSIX version (which returns int) or the GNU version
// (which returns char *).

#include <cstring>
#include <type_traits>

int main()
{
  char buffer[200];
  auto res{strerror_r(1, buffer, 200)};
  // Sidestep type differences.  We don't really care what the value is.
  return not not res;
}
