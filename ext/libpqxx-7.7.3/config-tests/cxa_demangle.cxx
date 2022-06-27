// Test for cross-vendor C++ ABI's __cxa_demangle function.
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <typeinfo>

#include <cxxabi.h>

int main()
{
  int status = 0;
  char *name =
    abi::__cxa_demangle(typeid(10).name(), nullptr, nullptr, &status);
  if (status != 0)
    throw std::runtime_error("Demangle failed!");
  int result = std::strcmp(name, "int");
  std::free(name);
  return result;
}
