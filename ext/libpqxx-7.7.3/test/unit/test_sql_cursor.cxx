#include <pqxx/cursor>
#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
void test_forward_sql_cursor()
{
  pqxx::connection conn;
  pqxx::work tx{conn};

  // Plain owned, scoped, forward-only read-only cursor.
  pqxx::internal::sql_cursor forward(
    tx, "SELECT generate_series(1, 4)", "forward",
    pqxx::cursor_base::forward_only, pqxx::cursor_base::read_only,
    pqxx::cursor_base::owned, false);

  PQXX_CHECK_EQUAL(forward.pos(), 0, "Wrong initial position");
  PQXX_CHECK_EQUAL(forward.endpos(), -1, "Wrong initial endpos()");

  auto empty_result{forward.empty_result()};
  PQXX_CHECK_EQUAL(std::size(empty_result), 0, "Empty result not empty");

  auto displacement{0};
  auto one{forward.fetch(1, displacement)};
  PQXX_CHECK_EQUAL(std::size(one), 1, "Fetched wrong number of rows");
  PQXX_CHECK_EQUAL(one[0][0].as<std::string>(), "1", "Unexpected result");
  PQXX_CHECK_EQUAL(displacement, 1, "Wrong displacement");
  PQXX_CHECK_EQUAL(forward.pos(), 1, "In wrong position");

  auto offset{forward.move(1, displacement)};
  PQXX_CHECK_EQUAL(offset, 1, "Unexpected offset from move()");
  PQXX_CHECK_EQUAL(displacement, 1, "Unexpected displacement after move()");
  PQXX_CHECK_EQUAL(forward.pos(), 2, "Wrong position after move()");
  PQXX_CHECK_EQUAL(forward.endpos(), -1, "endpos() unexpectedly set");

  auto row{forward.fetch(0, displacement)};
  PQXX_CHECK_EQUAL(std::size(row), 0, "fetch(0, displacement) returns rows");
  PQXX_CHECK_EQUAL(displacement, 0, "Unexpected displacement after fetch(0)");
  PQXX_CHECK_EQUAL(forward.pos(), 2, "fetch(0, displacement) affected pos()");

  row = forward.fetch(0);
  PQXX_CHECK_EQUAL(std::size(row), 0, "fetch(0) fetched wrong number of rows");
  PQXX_CHECK_EQUAL(forward.pos(), 2, "fetch(0) moved cursor");
  PQXX_CHECK_EQUAL(forward.pos(), 2, "fetch(0) affected pos()");

  offset = forward.move(1);
  PQXX_CHECK_EQUAL(offset, 1, "move(1) returned unexpected value");
  PQXX_CHECK_EQUAL(forward.pos(), 3, "move(1) after fetch(0) broke");

  row = forward.fetch(1);
  PQXX_CHECK_EQUAL(
    std::size(row), 1, "fetch(1) returned wrong number of rows");
  PQXX_CHECK_EQUAL(forward.pos(), 4, "fetch(1) results in bad pos()");
  PQXX_CHECK_EQUAL(row[0][0].as<std::string>(), "4", "pos() is lying");

  empty_result = forward.fetch(1, displacement);
  PQXX_CHECK_EQUAL(std::size(empty_result), 0, "Got rows at end of cursor");
  PQXX_CHECK_EQUAL(forward.pos(), 5, "Not at one-past-end position");
  PQXX_CHECK_EQUAL(forward.endpos(), 5, "Failed to notice end position");
  PQXX_CHECK_EQUAL(displacement, 1, "Wrong displacement at end position");

  offset = forward.move(5, displacement);
  PQXX_CHECK_EQUAL(offset, 0, "move() lied at end of result set");
  PQXX_CHECK_EQUAL(forward.pos(), 5, "pos() is beyond end");
  PQXX_CHECK_EQUAL(forward.endpos(), 5, "endpos() changed after end position");
  PQXX_CHECK_EQUAL(displacement, 0, "Wrong displacement after end position");

  // Move through entire result set at once.
  pqxx::internal::sql_cursor forward2(
    tx, "SELECT generate_series(1, 4)", "forward",
    pqxx::cursor_base::forward_only, pqxx::cursor_base::read_only,
    pqxx::cursor_base::owned, false);

  // Move through entire result set at once.
  offset = forward2.move(pqxx::cursor_base::all(), displacement);
  PQXX_CHECK_EQUAL(offset, 4, "Unexpected number of rows in result set");
  PQXX_CHECK_EQUAL(displacement, 5, "displacement != rows+1");
  PQXX_CHECK_EQUAL(forward2.pos(), 5, "Bad pos() after skipping all rows");
  PQXX_CHECK_EQUAL(forward2.endpos(), 5, "Bad endpos() after skipping");

  pqxx::internal::sql_cursor forward3(
    tx, "SELECT generate_series(1, 4)", "forward",
    pqxx::cursor_base::forward_only, pqxx::cursor_base::read_only,
    pqxx::cursor_base::owned, false);

  // Fetch entire result set at once.
  auto rows{forward3.fetch(pqxx::cursor_base::all(), displacement)};
  PQXX_CHECK_EQUAL(
    std::size(rows), 4, "Unexpected number of rows in result set");
  PQXX_CHECK_EQUAL(displacement, 5, "displacement != rows+1");
  PQXX_CHECK_EQUAL(forward3.pos(), 5, "Bad pos() after fetching all rows");
  PQXX_CHECK_EQUAL(forward3.endpos(), 5, "Bad endpos() after fetching");

  pqxx::internal::sql_cursor forward_empty(
    tx, "SELECT generate_series(0, -1)", "forward_empty",
    pqxx::cursor_base::forward_only, pqxx::cursor_base::read_only,
    pqxx::cursor_base::owned, false);

  offset = forward_empty.move(3, displacement);
  PQXX_CHECK_EQUAL(forward_empty.pos(), 1, "Bad pos() at end of result");
  PQXX_CHECK_EQUAL(forward_empty.endpos(), 1, "Bad endpos() in empty result");
  PQXX_CHECK_EQUAL(displacement, 1, "Bad displacement in empty result");
  PQXX_CHECK_EQUAL(offset, 0, "move() in empty result counted rows");
}

void test_scroll_sql_cursor()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::internal::sql_cursor scroll(
    tx, "SELECT generate_series(1, 10)", "scroll",
    pqxx::cursor_base::random_access, pqxx::cursor_base::read_only,
    pqxx::cursor_base::owned, false);

  PQXX_CHECK_EQUAL(scroll.pos(), 0, "Scroll cursor's initial pos() is wrong");
  PQXX_CHECK_EQUAL(scroll.endpos(), -1, "New scroll cursor has endpos() set");

  auto rows{scroll.fetch(pqxx::cursor_base::next())};
  PQXX_CHECK_EQUAL(std::size(rows), 1, "Scroll cursor is broken");
  PQXX_CHECK_EQUAL(scroll.pos(), 1, "Scroll cursor's pos() is broken");
  PQXX_CHECK_EQUAL(scroll.endpos(), -1, "endpos() set prematurely");

  // Turn cursor around.  This is where we begin to feel SQL cursors'
  // semantics: we pre-decrement, ending up on the position in front of the
  // first row and returning no rows.
  rows = scroll.fetch(pqxx::cursor_base::prior());
  PQXX_CHECK_EQUAL(std::empty(rows), true, "Turning around on fetch() broke");
  PQXX_CHECK_EQUAL(scroll.pos(), 0, "pos() is not back at zero");
  PQXX_CHECK_EQUAL(
    scroll.endpos(), -1, "endpos() set on wrong side of result");

  // Bounce off the left-hand side of the result set.  Can't move before the
  // starting position.
  auto offset{0}, displacement{0};
  offset = scroll.move(-3, displacement);
  PQXX_CHECK_EQUAL(offset, 0, "Rows found before beginning");
  PQXX_CHECK_EQUAL(displacement, 0, "Failed to bounce off beginning");
  PQXX_CHECK_EQUAL(scroll.pos(), 0, "pos() moved back from zero");
  PQXX_CHECK_EQUAL(scroll.endpos(), -1, "endpos() set on left-side bounce");

  // Try bouncing off the left-hand side a little harder.  Take 4 paces away
  // from the boundary and run into it.
  offset = scroll.move(4, displacement);
  PQXX_CHECK_EQUAL(offset, 4, "Offset mismatch");
  PQXX_CHECK_EQUAL(displacement, 4, "Displacement mismatch");
  PQXX_CHECK_EQUAL(scroll.pos(), 4, "Position mismatch");
  PQXX_CHECK_EQUAL(scroll.endpos(), -1, "endpos() set at weird time");

  offset = scroll.move(-10, displacement);
  PQXX_CHECK_EQUAL(offset, 3, "Offset mismatch");
  PQXX_CHECK_EQUAL(displacement, -4, "Displacement mismatch");
  PQXX_CHECK_EQUAL(scroll.pos(), 0, "Hard bounce failed");
  PQXX_CHECK_EQUAL(scroll.endpos(), -1, "endpos() set during hard bounce");

  rows = scroll.fetch(3);
  PQXX_CHECK_EQUAL(scroll.pos(), 3, "Bad pos()");
  PQXX_CHECK_EQUAL(std::size(rows), 3, "Wrong number of rows");
  PQXX_CHECK_EQUAL(rows[2][0].as<int>(), 3, "pos() does not match data");
  rows = scroll.fetch(-1);
  PQXX_CHECK_EQUAL(scroll.pos(), 2, "Bad pos()");
  PQXX_CHECK_EQUAL(rows[0][0].as<int>(), 2, "pos() does not match data");

  rows = scroll.fetch(1);
  PQXX_CHECK_EQUAL(scroll.pos(), 3, "Bad pos() after inverse turnaround");
  PQXX_CHECK_EQUAL(rows[0][0].as<int>(), 3, "Data position mismatch");
}


void test_adopted_sql_cursor()
{
  pqxx::connection conn;
  pqxx::work tx{conn};

  tx.exec0(
    "DECLARE adopted SCROLL CURSOR FOR "
    "SELECT generate_series(1, 3)");
  pqxx::internal::sql_cursor adopted(tx, "adopted", pqxx::cursor_base::owned);
  PQXX_CHECK_EQUAL(adopted.pos(), -1, "Adopted cursor has known pos()");
  PQXX_CHECK_EQUAL(adopted.endpos(), -1, "Adopted cursor has known endpos()");

  auto displacement{0};
  auto rows{adopted.fetch(pqxx::cursor_base::all(), displacement)};
  PQXX_CHECK_EQUAL(std::size(rows), 3, "Wrong number of rows in result");
  PQXX_CHECK_EQUAL(rows[0][0].as<int>(), 1, "Wrong result data");
  PQXX_CHECK_EQUAL(rows[2][0].as<int>(), 3, "Wrong result data");
  PQXX_CHECK_EQUAL(displacement, 4, "Wrong displacement");
  PQXX_CHECK_EQUAL(
    adopted.pos(), -1, "End-of-result set pos() on adopted cur");
  PQXX_CHECK_EQUAL(adopted.endpos(), -1, "endpos() set too early");

  rows = adopted.fetch(pqxx::cursor_base::backward_all(), displacement);
  PQXX_CHECK_EQUAL(std::size(rows), 3, "Wrong number of rows in result");
  PQXX_CHECK_EQUAL(rows[0][0].as<int>(), 3, "Wrong result data");
  PQXX_CHECK_EQUAL(rows[2][0].as<int>(), 1, "Wrong result data");
  PQXX_CHECK_EQUAL(displacement, -4, "Wrong displacement");
  PQXX_CHECK_EQUAL(adopted.pos(), 0, "Failed to recognize starting position");
  PQXX_CHECK_EQUAL(adopted.endpos(), -1, "endpos() set too early");

  auto offset{adopted.move(pqxx::cursor_base::all())};
  PQXX_CHECK_EQUAL(offset, 3, "Unexpected move() offset");
  PQXX_CHECK_EQUAL(adopted.pos(), 4, "Bad position on adopted cursor");
  PQXX_CHECK_EQUAL(adopted.endpos(), 4, "endpos() not set properly");

  // Owned adopted cursors are cleaned up on destruction.
  pqxx::connection conn2;
  pqxx::work tx2(conn2, "tx2");
  tx2.exec0(
    "DECLARE adopted2 CURSOR FOR "
    "SELECT generate_series(1, 3)");
  {
    pqxx::internal::sql_cursor(tx2, "adopted2", pqxx::cursor_base::owned);
  }
  // Modern backends: accessing the cursor now is an error, as you'd expect.
  PQXX_CHECK_THROWS(
    tx2.exec("FETCH 1 IN adopted2"), pqxx::sql_error,
    "Owned adopted cursor not cleaned up");

  tx2.abort();

  pqxx::work tx3(conn2, "tx3");
  tx3.exec(
    "DECLARE adopted3 CURSOR FOR "
    "SELECT generate_series(1, 3)");
  {
    pqxx::internal::sql_cursor(tx3, "adopted3", pqxx::cursor_base::loose);
  }
  tx3.exec("MOVE 1 IN adopted3");
}

void test_hold_cursor()
{
  pqxx::connection conn;
  pqxx::work tx{conn};

  // "With hold" cursor is kept after commit.
  pqxx::internal::sql_cursor with_hold(
    tx, "SELECT generate_series(1, 3)", "hold_cursor",
    pqxx::cursor_base::forward_only, pqxx::cursor_base::read_only,
    pqxx::cursor_base::owned, true);
  tx.commit();
  pqxx::work tx2(conn, "tx2");
  auto rows{with_hold.fetch(1)};
  PQXX_CHECK_EQUAL(
    std::size(rows), 1, "Did not get 1 row from with-hold cursor");

  // Cursor without hold is closed on commit.
  pqxx::internal::sql_cursor no_hold(
    tx2, "SELECT generate_series(1, 3)", "no_hold_cursor",
    pqxx::cursor_base::forward_only, pqxx::cursor_base::read_only,
    pqxx::cursor_base::owned, false);
  tx2.commit();
  pqxx::work tx3(conn, "tx3");
  PQXX_CHECK_THROWS(
    no_hold.fetch(1), pqxx::sql_error, "Cursor not closed on commit");
}


void cursor_tests()
{
  test_forward_sql_cursor();
  test_scroll_sql_cursor();
  test_adopted_sql_cursor();
  test_hold_cursor();
}


PQXX_REGISTER_TEST(cursor_tests);
} // namespace
