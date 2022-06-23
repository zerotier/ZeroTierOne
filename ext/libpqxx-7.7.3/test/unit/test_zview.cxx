#include <pqxx/zview>

#include "../test_helpers.hxx"


namespace
{
void test_zview_literal()
{
  using pqxx::operator"" _zv;

  PQXX_CHECK_EQUAL(("foo"_zv), pqxx::zview{"foo"}, "zview literal is broken.");
}

PQXX_REGISTER_TEST(test_zview_literal);
} // namespace
