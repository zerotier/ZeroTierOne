#include <iostream>

#include <pqxx/nontransaction>

#include "test_helpers.hxx"


// Test program for libpqxx.  Read and print table using row iterators.
namespace
{
void test_082()
{
  pqxx::connection conn;
  pqxx::nontransaction tx{conn};

  pqxx::test::create_pqxxevents(tx);
  std::string const Table{"pqxxevents"};
  pqxx::result R{tx.exec("SELECT * FROM " + Table)};

  PQXX_CHECK(not std::empty(R), "Got empty result.");

  std::string const nullstr("[null]");

  for (auto const &r : R)
  {
    pqxx::row::const_iterator f2(r[0]);
    for (auto const &f : r)
    {
      PQXX_CHECK_EQUAL(
        (*f2).as(nullstr), f.as(nullstr), "Inconsistent iteration result.");
      ++f2;
    }

    PQXX_CHECK(
      std::begin(r) + pqxx::row::difference_type(std::size(r)) == std::end(r),
      "Row end() appears to be in the wrong place.");
    PQXX_CHECK(
      pqxx::row::difference_type(std::size(r)) + std::begin(r) == std::end(r),
      "Row iterator addition is not commutative.");
    PQXX_CHECK_EQUAL(
      std::begin(r)->num(), 0, "Wrong column number at begin().");

    pqxx::row::const_iterator f3(r[std::size(r)]);

    PQXX_CHECK(f3 == std::end(r), "Did not get end() at end of row.");

    PQXX_CHECK(
      f3 > std::begin(r), "Row end() appears to precede its begin().");

    PQXX_CHECK(
      f3 >= std::end(r) and std::begin(r) < f3,
      "Row iterator operator<() is broken.");

    PQXX_CHECK(f3 > std::begin(r), "Row end() not greater than begin().");

    pqxx::row::const_iterator f4{r, std::size(r)};
    PQXX_CHECK(f4 == f3, "Row iterator constructor with offset is broken.");

    --f3;
    f4 -= 1;

    PQXX_CHECK(f3 < std::end(r), "Last field in row is not before end().");
    PQXX_CHECK(f3 >= std::begin(r), "Last field in row precedes begin().");
    PQXX_CHECK(
      f3 == std::end(r) - 1, "Back from end() doese not yield end()-1.");
    PQXX_CHECK_EQUAL(
      std::end(r) - f3, 1, "Wrong distance from last row to end().");

    PQXX_CHECK(f4 == f3, "Row iterator operator-=() is broken.");
    f4 += 1;
    PQXX_CHECK(f4 == std::end(r), "Row iterator operator+=() is broken.");

    for (auto fr = std::rbegin(r); fr != std::rend(r); ++fr, --f3)
      PQXX_CHECK_EQUAL(
        *fr, *f3,
        "Reverse traversal is not consistent with forward traversal.");
  }

  // Thorough test for row::const_reverse_iterator
  pqxx::row::const_reverse_iterator ri1(std::rbegin(R.front())), ri2(ri1),
    ri3(std::end(R.front()));
  ri2 = std::rbegin(R.front());

  PQXX_CHECK(
    ri1 == ri2, "Copy-constructed reverse_iterator is not equal to original.");

  PQXX_CHECK(ri2 == ri3, "result::end() does not generate rbegin().");
  PQXX_CHECK_EQUAL(
    ri2 - ri3, 0,
    "Distance between identical const_reverse_iterators was nonzero.");

  PQXX_CHECK(
    pqxx::row::const_reverse_iterator(ri1.base()) == ri1,
    "Back-conversion of reverse_iterator base() fails.");

  PQXX_CHECK(ri2 == ri3 + 0, "reverse_iterator+0 gives strange result.");
  PQXX_CHECK(ri2 == ri3 - 0, "reverse_iterator-0 gives strange result.");

  PQXX_CHECK(
    not(ri3 < ri2),
    "reverse_iterator operator<() breaks on identical iterators.");
  PQXX_CHECK(
    ri2 <= ri3,
    "reverse_iterator operator<=() breaks on identical iterators.");
  PQXX_CHECK(ri3++ == ri2, "reverse_iterator post-increment is broken.");

  PQXX_CHECK_EQUAL(ri3 - ri2, 1, "Wrong reverse_iterator distance.");
  PQXX_CHECK(ri3 > ri2, "reverse_iterator operator>() is broken.");
  PQXX_CHECK(ri3 >= ri2, "reverse_iterator operator>=() is broken.");
  PQXX_CHECK(ri2 < ri3, "reverse_iterator operator<() is broken.");
  PQXX_CHECK(ri2 <= ri3, "reverse_iterator operator<=() is broken.");
  PQXX_CHECK(ri3 == ri2 + 1, "Adding number to reverse_iterator goes wrong.");
  PQXX_CHECK(ri2 == ri3 - 1, "Subtracting from reverse_iterator goes wrong.");

  PQXX_CHECK(
    ri3 == ++ri2, "reverse_iterator pre-incremen returns wrong result.");

  PQXX_CHECK(
    ri3 >= ri2, "reverse_iterator operator>=() breaks on equal iterators.");
  PQXX_CHECK(
    ri3 >= ri2, "reverse_iterator operator<=() breaks on equal iterators.");
  PQXX_CHECK(
    *ri3.base() == R.front().back(),
    "reverse_iterator does not arrive at back().");
  PQXX_CHECK(
    ri1->c_str()[0] == (*ri1).c_str()[0],
    "reverse_iterator operator->() is inconsistent with operator*().");
  PQXX_CHECK(
    ri2-- == ri3, "reverse_iterator post-decrement returns wrong result.");
  PQXX_CHECK(
    ri2 == --ri3, "reverse_iterator pre-increment returns wrong result.");
  PQXX_CHECK(
    ri2 == std::rbegin(R.front()),
    "Moving iterator back and forth doesn't get it back to origin.");

  ri2 += 1;
  ri3 -= -1;

  PQXX_CHECK(
    ri2 != std::rbegin(R.front()), "Adding to reverse_iterator doesn't work.");
  PQXX_CHECK(
    ri2 != std::rbegin(R.front()), "Adding to reverse_iterator doesn't work.");
  PQXX_CHECK(
    ri3 == ri2, "reverse_iterator operator-=() breaks on negative numbers.");

  ri2 -= 1;
  PQXX_CHECK(
    ri2 == std::rbegin(R.front()),
    "reverse_iterator operator+=() and operator-=() do not cancel out");
}
} // namespace


PQXX_REGISTER_TEST(test_082);
