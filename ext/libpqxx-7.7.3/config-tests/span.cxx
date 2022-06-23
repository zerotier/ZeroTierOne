// Test for std::span.
#include <span>

int main(int argc, char **argv)
{
  std::span<char *> args{argv, static_cast<std::size_t>(argc)};
  return static_cast<int>(std::size(args) - 1u);
}
