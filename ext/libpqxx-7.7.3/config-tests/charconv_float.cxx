// Test for std::to_string/std::from_string for floating-point types.
#include <charconv>
#include <iterator>

int main()
{
  char z[100];
  auto rt = std::to_chars(std::begin(z), std::end(z), 3.14159L);
  if (rt.ec != std::errc{})
    return 1;
  long double n;
  auto rf = std::from_chars(std::cbegin(z), std::cend(z), n);
  if (rf.ec != std::errc{})
    return 2;
  return (n > 3 and n < 4) ? 0 : 1;
}
