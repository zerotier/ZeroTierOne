// Check for working std::filesystem support.
#include <filesystem>


int main()
{
  // Apparently some versions of MinGW lack this comparison operator.
  return std::filesystem::path{} != std::filesystem::path{};
}
