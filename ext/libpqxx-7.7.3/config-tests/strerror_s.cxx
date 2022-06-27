// Test for strerror_s, as defined in Windows and C11.
// Presumably this'll be part of the C++ standard some day.

#include <cstring>

int main()
{
  using namespace std;
  char buf[200];
  return strerror_s(buf, 200, 1);
}
