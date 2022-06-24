// Test for C++20 std::cmp_greater etc. support.
#include <utility>


int main()
{
  return std::cmp_greater(-1, 2u) && std::cmp_less_equal(3, 0);
}
