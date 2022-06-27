#include <iostream>
#include <vector>

#include <pqxx/transaction>

#include "test_helpers.hxx"


// Test program for libpqxx.  Compare const_reverse_iterator iteration of a
// result to a regular, const_iterator iteration.
namespace
{
void test_075()
{
  pqxx::connection conn;
  pqxx::work tx{conn};

  pqxx::test::create_pqxxevents(tx);
  auto const R(tx.exec("SELECT year FROM pqxxevents"));
  PQXX_CHECK(not std::empty(R), "No events found, cannot test.");

  PQXX_CHECK_EQUAL(R[0], R.at(0), "Inconsistent result indexing.");
  PQXX_CHECK(not(R[0] != R.at(0)), "result::row::operator!=() is broken.");

  PQXX_CHECK_EQUAL(R[0][0], R[0].at(0), "Inconsistent row indexing.");
  PQXX_CHECK(
    not(R[0][0] != R[0].at(0)), "result::field::operator!=() is broken.");

  std::vector<std::string> contents;
  for (auto const &i : R) contents.push_back(i.at(0).as<std::string>());

  PQXX_CHECK_EQUAL(
    std::size(contents), std::vector<std::string>::size_type(std::size(R)),
    "Number of values does not match result size.");

  for (pqxx::result::size_type i{0}; i < std::size(R); ++i)
    PQXX_CHECK_EQUAL(
      contents[static_cast<std::size_t>(i)], R.at(i).at(0).c_str(),
      "Inconsistent iteration.");

  // Thorough test for result::const_reverse_iterator
  pqxx::result::const_reverse_iterator ri1(std::rbegin(R)), ri2(ri1),
    ri3(std::end(R));
  ri2 = std::rbegin(R);

  PQXX_CHECK(ri2 == ri1, "reverse_iterator copy constructor is broken.");
  PQXX_CHECK(ri3 == ri2, "result::end() does not generate rbegin().");
  PQXX_CHECK_EQUAL(
    ri2 - ri3, 0,
    "const_reverse_iterator is at nonzero distance from its own copy.");

  PQXX_CHECK(ri2 == ri3 + 0, "reverse_iterator+0 gives strange result.");
  PQXX_CHECK(ri2 == ri3 - 0, "reverse_iterator-0 gives strange result.");
  PQXX_CHECK(not(ri3 < ri2), "operator<() breaks on equal reverse_iterators.");
  PQXX_CHECK(ri2 <= ri3, "operator<=() breaks on equal reverse_iterators.");

  PQXX_CHECK(ri3++ == ri2, "reverse_iterator post-increment is broken.");

  PQXX_CHECK_EQUAL(ri3 - ri2, 1, "Wrong nonzero reverse_iterator distance.");
  PQXX_CHECK(ri3 > ri2, "reverse_iterator operator>() is broken.");
  PQXX_CHECK(ri3 >= ri2, "reverse_iterator operator>=() is broken.");
  PQXX_CHECK(ri2 < ri3, "reverse_iterator operator<() is broken.");
  PQXX_CHECK(ri2 <= ri3, "reverse_iterator operator<=() is broken.");
  PQXX_CHECK(ri3 == ri2 + 1, "Adding int to reverse_iterator is broken.");
  PQXX_CHECK(
    ri2 == ri3 - 1, "Subtracting int from reverse_iterator is broken.");

  PQXX_CHECK(ri3 == ++ri2, "reverse_iterator pre-increment is broken.");
  PQXX_CHECK(ri3 >= ri2, "operator>=() breaks on equal reverse_iterators.");
  PQXX_CHECK(ri3 >= ri2, "operator<=() breaks on equal reverse_iterators.");

  PQXX_CHECK(
    *ri3.base() == R.back(), "reverse_iterator does not arrive at back().");

  PQXX_CHECK(
    ri1->at(0) == (*ri1).at(0),
    "reverse_iterator operator->() is inconsistent with operator*().");

  PQXX_CHECK(ri2-- == ri3, "reverse_iterator post-decrement is broken.");
  PQXX_CHECK(ri2 == --ri3, "reverse_iterator pre-decrement is broken.");
  PQXX_CHECK(ri2 == std::rbegin(R), "reverse_iterator decrement is broken.");

  ri2 += 1;
  ri3 -= -1;

  PQXX_CHECK(
    ri2 != std::rbegin(R), "Adding to reverse_iterator does not work.");
  PQXX_CHECK(
    ri3 == ri2, "reverse_iterator operator-=() breaks on negative distances.");

  ri2 -= 1;
  PQXX_CHECK(
    ri2 == std::rbegin(R),
    "reverse_iterator operator+=() and operator-=() do not cancel out.");

  // Now verify that reverse iterator also sees the same results...
  auto l{std::rbegin(contents)};
  for (auto i{std::rbegin(R)}; i != std::rend(R); ++i, ++l)
    PQXX_CHECK_EQUAL(*l, i->at(0).c_str(), "Inconsistent reverse iteration.");

  PQXX_CHECK(l == std::rend(contents), "Reverse iteration ended too soon.");

  PQXX_CHECK(not std::empty(R), "No events found in table, cannot test.");
}
} // namespace


PQXX_REGISTER_TEST(test_075);
