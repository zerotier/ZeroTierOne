// Test for std::to_string/std::from_string for floating-point types.
#include <iostream>
#include <sstream>

int main(int argc, char **)
{
#if defined(__MINGW32__) && defined(__GNUC__)
#  if __GNUC__ < 11 || ((__GNUC__ == 11) && (__GNU_MINOR__ == 0))
#    error "On MinGW before gcc 11.1, thread_local breaks at run time."
#  endif
#endif
  thread_local std::stringstream s;
  s << argc;
  std::cout << s.str();
}
