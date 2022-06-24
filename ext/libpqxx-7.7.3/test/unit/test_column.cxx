#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
void test_table_column()
{
  pqxx::connection conn;
  pqxx::work tx{conn};

  tx.exec0("CREATE TEMP TABLE pqxxfoo (x varchar, y integer, z integer)");
  tx.exec0("INSERT INTO pqxxfoo VALUES ('xx', 1, 2)");
  auto R{tx.exec("SELECT z,y,x FROM pqxxfoo")};
  auto X{tx.exec("SELECT x,y,z,99 FROM pqxxfoo")};

  pqxx::row::size_type x{R.table_column(2)}, y{R.table_column(1)},
    z{R.table_column(static_cast<int>(0))};

  PQXX_CHECK_EQUAL(x, 0, "Wrong column number.");
  PQXX_CHECK_EQUAL(y, 1, "Wrong column number.");
  PQXX_CHECK_EQUAL(z, 2, "Wrong column number.");

  x = R.table_column("x");
  y = R.table_column("y");
  z = R.table_column("z");

  PQXX_CHECK_EQUAL(x, 0, "Wrong number for named column.");
  PQXX_CHECK_EQUAL(y, 1, "Wrong number for named column.");
  PQXX_CHECK_EQUAL(z, 2, "Wrong number for named column.");

  pqxx::row::size_type xx{X[0].table_column(static_cast<int>(0))},
    yx{X[0].table_column(pqxx::row::size_type(1))}, zx{X[0].table_column("z")};

  PQXX_CHECK_EQUAL(xx, 0, "Bad result from table_column(int).");
  PQXX_CHECK_EQUAL(yx, 1, "Bad result from table_column(size_type).");
  PQXX_CHECK_EQUAL(zx, 2, "Bad result from table_column(string).");

  for (pqxx::row::size_type i{0}; i < std::size(R[0]); ++i)
    PQXX_CHECK_EQUAL(
      R[0][i].table_column(), R.table_column(i),
      "Bad result from column_table().");

  int col;
  PQXX_CHECK_THROWS_EXCEPTION(
    col = R.table_column(3), "table_column() with invalid index didn't fail.");
  pqxx::ignore_unused(col);

  PQXX_CHECK_THROWS_EXCEPTION(
    col = R.table_column("nonexistent"),
    "table_column() with invalid column name didn't fail.");
  pqxx::ignore_unused(col);

  PQXX_CHECK_THROWS_EXCEPTION(
    col = X.table_column(3), "table_column() on non-table didn't fail.");
  pqxx::ignore_unused(col);
}
} // namespace


PQXX_REGISTER_TEST(test_table_column);
