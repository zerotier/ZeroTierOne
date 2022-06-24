#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
void test_row()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::result rows{tx.exec("SELECT 1, 2, 3")};
  pqxx::row r{rows[0]};
  PQXX_CHECK_EQUAL(std::size(r), 3, "Unexpected row size.");
  PQXX_CHECK_EQUAL(r.at(0).as<int>(), 1, "Wrong value at index 0.");
  PQXX_CHECK(std::begin(r) != std::end(r), "Broken row iteration.");
  PQXX_CHECK(std::begin(r) < std::end(r), "Row begin does not precede end.");
  PQXX_CHECK(std::cbegin(r) == std::begin(r), "Wrong cbegin.");
  PQXX_CHECK(std::cend(r) == std::end(r), "Wrong cend.");
  PQXX_CHECK(std::rbegin(r) != std::rend(r), "Broken reverse row iteration.");
  PQXX_CHECK(std::crbegin(r) == std::rbegin(r), "Wrong crbegin.");
  PQXX_CHECK(std::crend(r) == std::rend(r), "Wrong crend.");
  PQXX_CHECK_EQUAL(r.front().as<int>(), 1, "Wrong row front().");
  PQXX_CHECK_EQUAL(r.back().as<int>(), 3, "Wrong row back().");
}


void test_row_iterator()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::result rows{tx.exec("SELECT 1, 2, 3")};

  auto i{std::begin(rows[0])};
  PQXX_CHECK_EQUAL(i->as<int>(), 1, "Row iterator is wrong.");
  auto i2{i};
  PQXX_CHECK_EQUAL(i2->as<int>(), 1, "Row iterator copy is wrong.");
  i2++;
  PQXX_CHECK_EQUAL(i2->as<int>(), 2, "Row iterator increment is wrong.");
  pqxx::row::const_iterator i3;
  i3 = i2;
  PQXX_CHECK_EQUAL(i3->as<int>(), 2, "Row iterator assignment is wrong.");

  auto r{std::rbegin(rows[0])};
  PQXX_CHECK_EQUAL(r->as<int>(), 3, "Row reverse iterator is wrong.");
  auto r2{r};
  PQXX_CHECK_EQUAL(r2->as<int>(), 3, "Row reverse iterator copy is wrong.");
  r2++;
  PQXX_CHECK_EQUAL(
    r2->as<int>(), 2, "Row reverse iterator increment is wrong.");
  pqxx::row::const_reverse_iterator r3;
  r3 = r2;
  PQXX_CHECK_EQUAL(
    i3->as<int>(), 2, "Row reverse iterator assignment is wrong.");
}


void test_row_as()
{
  using pqxx::operator"" _zv;

  pqxx::connection conn;
  pqxx::work tx{conn};

  pqxx::row const r{tx.exec1("SELECT 1, 2, 3")};
  auto [one, two, three]{r.as<int, float, pqxx::zview>()};
  static_assert(std::is_same_v<decltype(one), int>);
  static_assert(std::is_same_v<decltype(two), float>);
  static_assert(std::is_same_v<decltype(three), pqxx::zview>);
  PQXX_CHECK_EQUAL(one, 1, "row::as() did not produce the right int.");
  PQXX_CHECK_GREATER(two, 1.9, "row::as() did not produce the right float.");
  PQXX_CHECK_LESS(two, 2.1, "row::as() did not produce the right float.");
  PQXX_CHECK_EQUAL(
    three, "3"_zv, "row::as() did not produce the right zview.");

  PQXX_CHECK_EQUAL(
    std::get<0>(tx.exec1("SELECT 999").as<int>()), 999,
    "Unary tuple did not extract right.");
}


PQXX_REGISTER_TEST(test_row);
PQXX_REGISTER_TEST(test_row_iterator);
PQXX_REGISTER_TEST(test_row_as);
} // namespace
