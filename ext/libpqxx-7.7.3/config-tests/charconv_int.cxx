// Test for std::to_string/std::from_string for integral types.
#include <charconv>
#include <iterator>

int main()
{
  char z[100];
  auto rt = std::to_chars(std::begin(z), std::end(z), 9ULL);
  if (rt.ec != std::errc{})
    return 1;
  unsigned long long n;
  auto rf = std::from_chars(std::cbegin(z), std::cend(z), n);
  if (rf.ec != std::errc{})
    return 2;
  return (n == 9ULL) ? 0 : 1;
}
