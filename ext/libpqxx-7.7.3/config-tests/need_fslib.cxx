// Check whether we need to link to the stdc++fs library.
//
// We assume that the presence of the <filesystem> header means that we have
// support for the basics of std::filesystem.  This check will succeed if
// either there is no <filesystem> header, or there is one and it works without
// any special options.  If the link fails, we assume that -lstdc++fs will fix
// it for us.

#include <iostream>

#if __has_include(<filesystem>)
#  include <filesystem>
#endif


int main()
{
#if __has_include(<filesystem>)
  std::cout << std::filesystem::path{"foo.bar"}.c_str() << '\n';
#endif
}
