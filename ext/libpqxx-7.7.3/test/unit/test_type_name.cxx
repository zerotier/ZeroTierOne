#include "../test_helpers.hxx"

namespace
{
void test_type_name()
{
  // It's hard to test in more detail, because spellings may differ.
  // For instance, one compiler might call "const unsigned int*" what another
  // might call "unsigned const *".  And Visual Studio prefixes "class" to
  // class types.
  std::string const i{pqxx::type_name<int>};
  PQXX_CHECK_LESS(std::size(i), 5u, "type_name<int> is suspiciously long.");
  PQXX_CHECK_EQUAL(
    i.substr(0, 1), "i", "type_name<int> does not start with 'i'.");
}


PQXX_REGISTER_TEST(test_type_name);
} // namespace
