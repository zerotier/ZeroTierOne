#include <pqxx/range>
#include <pqxx/strconv>

#include "../test_helpers.hxx"


namespace
{
void test_range_construct()
{
  using optint = std::optional<int>;
  using oibound = pqxx::inclusive_bound<std::optional<int>>;
  using oxbound = pqxx::inclusive_bound<std::optional<int>>;
  PQXX_CHECK_THROWS(
    (pqxx::range<optint>{oibound{optint{}}, oibound{optint{}}}),
    pqxx::argument_error, "Inclusive bound accepted a null.");
  PQXX_CHECK_THROWS(
    (pqxx::range<optint>{oxbound{optint{}}, oxbound{optint{}}}),
    pqxx::argument_error, "Exclusive bound accepted a null.");

  using ibound = pqxx::inclusive_bound<int>;
  PQXX_CHECK_THROWS(
    (pqxx::range<int>{ibound{1}, ibound{0}}), pqxx::range_error,
    "Range constructor accepted backwards range.");

  PQXX_CHECK_THROWS(
    (pqxx::range<float>{
      pqxx::inclusive_bound<float>{-1000.0},
      pqxx::inclusive_bound<float>{-std::numeric_limits<float>::infinity()}}),
    pqxx::range_error,
    "Was able to construct range with infinity bound at the wrong end.");
}


void test_range_equality()
{
  using range = pqxx::range<int>;
  using ibound = pqxx::inclusive_bound<int>;
  using xbound = pqxx::exclusive_bound<int>;
  using ubound = pqxx::no_bound;

  PQXX_CHECK_EQUAL(
    range{}, range{}, "Default-constructed range is not consistent.");
  PQXX_CHECK_EQUAL(
    (range{xbound{0}, xbound{0}}), (range{xbound{5}, xbound{5}}),
    "Empty ranges at different values are not equal.");

  PQXX_CHECK_EQUAL(
    (range{ubound{}, ubound{}}), (range{ubound{}, ubound{}}),
    "Universal range is inconsistent.");
  PQXX_CHECK_EQUAL(
    (range{ibound{5}, ibound{8}}), (range{ibound{5}, ibound{8}}),
    "Inclusive range is inconsistent.");
  PQXX_CHECK_EQUAL(
    (range{xbound{5}, xbound{8}}), (range{xbound{5}, xbound{8}}),
    "Exclusive range is inconsistent.");
  PQXX_CHECK_EQUAL(
    (range{xbound{5}, ibound{8}}), (range{xbound{5}, ibound{8}}),
    "Left-exclusive interval is not equal to itself.");
  PQXX_CHECK_EQUAL(
    (range{ibound{5}, xbound{8}}), (range{ibound{5}, xbound{8}}),
    "Right-exclusive interval is not equal to itself.");
  PQXX_CHECK_EQUAL(
    (range{ubound{}, ibound{8}}), (range{ubound{}, ibound{8}}),
    "Unlimited lower bound does not compare equal to same.");
  PQXX_CHECK_EQUAL(
    (range{ibound{8}, ubound{}}), (range{ibound{8}, ubound{}}),
    "Unlimited upper bound does not compare equal to same.");

  PQXX_CHECK_NOT_EQUAL(
    (range{ibound{5}, ibound{8}}), (range{xbound{5}, ibound{8}}),
    "Equality does not detect inclusive vs. exclusive lower bound.");
  PQXX_CHECK_NOT_EQUAL(
    (range{ibound{5}, ibound{8}}), (range{ubound{}, ibound{8}}),
    "Equality does not detect inclusive vs. unlimited lower bound.");
  PQXX_CHECK_NOT_EQUAL(
    (range{xbound{5}, ibound{8}}), (range{ubound{}, ibound{8}}),
    "Equality does not detect exclusive vs. unlimited lower bound.");
  PQXX_CHECK_NOT_EQUAL(
    (range{ibound{5}, ibound{8}}), (range{ibound{5}, xbound{8}}),
    "Equality does not detect inclusive vs. exclusive upper bound.");
  PQXX_CHECK_NOT_EQUAL(
    (range{ibound{5}, ibound{8}}), (range{ibound{5}, ubound{}}),
    "Equality does not detect inclusive vs. unlimited upper bound.");
  PQXX_CHECK_NOT_EQUAL(
    (range{ibound{5}, xbound{8}}), (range{ibound{5}, ubound{}}),
    "Equality does not detect exclusive vs. unlimited upper bound.");

  PQXX_CHECK_NOT_EQUAL(
    (range{ibound{5}, ibound{8}}), (range{ibound{4}, ibound{8}}),
    "Equality does not compare lower inclusive bound value.");
  PQXX_CHECK_NOT_EQUAL(
    (range{xbound{5}, ibound{8}}), (range{xbound{4}, ibound{8}}),
    "Equality does not compare lower exclusive bound value.");
  PQXX_CHECK_NOT_EQUAL(
    (range{xbound{5}, ibound{8}}), (range{xbound{5}, ibound{7}}),
    "Equality does not compare upper inclusive bound value.");
  PQXX_CHECK_NOT_EQUAL(
    (range{xbound{5}, xbound{8}}), (range{xbound{5}, xbound{7}}),
    "Equality does not compare lower exclusive bound value.");
}


void test_range_empty()
{
  using range = pqxx::range<int>;
  using ibound = pqxx::inclusive_bound<int>;
  using xbound = pqxx::exclusive_bound<int>;
  using ubound = pqxx::no_bound;
  PQXX_CHECK((range{}.empty()), "Default-constructed range is not empty.");
  PQXX_CHECK(
    (range{ibound{10}, xbound{10}}).empty(),
    "Right-exclusive zero-length interval is not empty.");
  PQXX_CHECK(
    (range{xbound{10}, ibound{10}}).empty(),
    "Left-exclusive zero-length interval is not empty.");
  PQXX_CHECK(
    (range{xbound{10}, xbound{10}}).empty(),
    "Exclusive zero-length interval is not empty.");

  PQXX_CHECK(
    not(range{ibound{10}, ibound{10}}).empty(),
    "Inclusive zero-length interval is empty.");
  PQXX_CHECK(
    not(range{xbound{10}, ibound{11}}.empty()),
    "Interval is incorrectly empty.");
  PQXX_CHECK(
    not(range{ubound{}, ubound{}}.empty()),
    "Double-unlimited interval is empty.");
  PQXX_CHECK(
    not(range{ubound{}, xbound{0}}.empty()),
    "Left-unlimited interval is empty.");
  PQXX_CHECK(
    not(range{xbound{0}, ubound{}}.empty()),
    "Right-unlimited interval is empty.");
}


void test_range_contains()
{
  using range = pqxx::range<int>;
  using ibound = pqxx::inclusive_bound<int>;
  using xbound = pqxx::exclusive_bound<int>;
  using ubound = pqxx::no_bound;

  PQXX_CHECK(not(range{}.contains(-1)), "Empty range contains a value.");
  PQXX_CHECK(not(range{}.contains(0)), "Empty range contains a value.");
  PQXX_CHECK(not(range{}.contains(1)), "Empty range contains a value.");

  PQXX_CHECK(
    not(range{ibound{5}, ibound{8}}.contains(4)),
    "Inclusive range contains value outside its left bound.");
  PQXX_CHECK(
    (range{ibound{5}, ibound{8}}.contains(5)),
    "Inclusive range does not contain value on its left bound.");
  PQXX_CHECK(
    (range{ibound{5}, ibound{8}}.contains(6)),
    "Inclusive range does not contain value inside it.");
  PQXX_CHECK(
    (range{ibound{5}, ibound{8}}.contains(8)),
    "Inclusive range does not contain value on its right bound.");
  PQXX_CHECK(
    not(range{ibound{5}, ibound{8}}.contains(9)),
    "Inclusive range contains value outside its right bound.");

  PQXX_CHECK(
    not(range{ibound{5}, xbound{8}}.contains(4)),
    "Left-inclusive range contains value outside its left bound.");
  PQXX_CHECK(
    (range{ibound{5}, xbound{8}}.contains(5)),
    "Left-inclusive range does not contain value on its left bound.");
  PQXX_CHECK(
    (range{ibound{5}, xbound{8}}.contains(6)),
    "Left-inclusive range does not contain value inside it.");
  PQXX_CHECK(
    not(range{ibound{5}, xbound{8}}.contains(8)),
    "Left-inclusive range contains value on its right bound.");
  PQXX_CHECK(
    not(range{ibound{5}, xbound{8}}.contains(9)),
    "Left-inclusive range contains value outside its right bound.");

  PQXX_CHECK(
    not(range{xbound{5}, ibound{8}}.contains(4)),
    "Right-inclusive range contains value outside its left bound.");
  PQXX_CHECK(
    not(range{xbound{5}, ibound{8}}.contains(5)),
    "Right-inclusive range does contains value on its left bound.");
  PQXX_CHECK(
    (range{xbound{5}, ibound{8}}.contains(6)),
    "Right-inclusive range does not contain value inside it.");
  PQXX_CHECK(
    (range{xbound{5}, ibound{8}}.contains(8)),
    "Right-inclusive range does not contain value on its right bound.");
  PQXX_CHECK(
    not(range{xbound{5}, ibound{8}}.contains(9)),
    "Right-inclusive range contains value outside its right bound.");

  PQXX_CHECK(
    not(range{xbound{5}, xbound{8}}.contains(4)),
    "Exclusive range contains value outside its left bound.");
  PQXX_CHECK(
    not(range{xbound{5}, xbound{8}}.contains(5)),
    "Exclusive range contains value on its left bound.");
  PQXX_CHECK(
    (range{xbound{5}, xbound{8}}.contains(6)),
    "Exclusive range does not contain value inside it.");
  PQXX_CHECK(
    not(range{xbound{5}, xbound{8}}.contains(8)),
    "Exclusive range does contains value on its right bound.");
  PQXX_CHECK(
    not(range{xbound{5}, xbound{8}}.contains(9)),
    "Exclusive range contains value outside its right bound.");

  PQXX_CHECK(
    (range{ubound{}, ibound{8}}.contains(7)),
    "Right-inclusive range does not contain value inside it.");
  PQXX_CHECK(
    (range{ubound{}, ibound{8}}.contains(8)),
    "Right-inclusive range does not contain value on its right bound.");
  PQXX_CHECK(
    not(range{ubound{}, ibound{8}}.contains(9)),
    "Right-inclusive range contains value outside its right bound.");

  PQXX_CHECK(
    (range{ubound{}, xbound{8}}.contains(7)),
    "Right-exclusive range does not contain value inside it.");
  PQXX_CHECK(
    not(range{ubound{}, xbound{8}}.contains(8)),
    "Right-exclusive range contains value on its right bound.");
  PQXX_CHECK(
    not(range{ubound{}, xbound{8}}.contains(9)),
    "Right-exclusive range contains value outside its right bound.");

  PQXX_CHECK(
    not(range{ibound{5}, ubound{}}.contains(4)),
    "Left-inclusive range contains value outside its left bound.");
  PQXX_CHECK(
    (range{ibound{5}, ubound{}}.contains(5)),
    "Left-inclusive range does not contain value on its left bound.");
  PQXX_CHECK(
    (range{ibound{5}, ubound{}}.contains(6)),
    "Left-inclusive range does not contain value inside it.");

  PQXX_CHECK(
    not(range{xbound{5}, ubound{}}.contains(4)),
    "Left-exclusive range contains value outside its left bound.");
  PQXX_CHECK(
    not(range{xbound{5}, ubound{}}.contains(5)),
    "Left-exclusive range contains value on its left bound.");
  PQXX_CHECK(
    (range{xbound{5}, ubound{}}.contains(6)),
    "Left-exclusive range does not contain value inside it.");

  PQXX_CHECK(
    (range{ubound{}, ubound{}}.contains(-1)), "Value not in universal range.");
  PQXX_CHECK(
    (range{ubound{}, ubound{}}.contains(0)), "Value not in universal range.");
  PQXX_CHECK(
    (range{ubound{}, ubound{}}.contains(1)), "Value not in universal range.");
}


void test_float_range_contains()
{
  using range = pqxx::range<double>;
  using ibound = pqxx::inclusive_bound<double>;
  using xbound = pqxx::exclusive_bound<double>;
  using ubound = pqxx::no_bound;
  using limits = std::numeric_limits<double>;
  constexpr auto inf{limits::infinity()};

  PQXX_CHECK(
    not(range{ibound{4.0}, ibound{8.0}}.contains(3.9)),
    "Float inclusive range contains value beyond its lower bound.");
  PQXX_CHECK(
    (range{ibound{4.0}, ibound{8.0}}.contains(4.0)),
    "Float inclusive range does not contain its lower bound value.");
  PQXX_CHECK(
    (range{ibound{4.0}, ibound{8.0}}.contains(5.0)),
    "Float inclusive range does not contain value inside it.");

  PQXX_CHECK(
    (range{ibound{0}, ibound{inf}}).contains(9999.0),
    "Range to infinity did not include large number.");
  PQXX_CHECK(
    not(range{ibound{0}, ibound{inf}}.contains(-0.1)),
    "Range to infinity includes number outside it.");
  PQXX_CHECK(
    (range{ibound{0}, xbound{inf}}.contains(9999.0)),
    "Range to exclusive infinity did not include large number.");
  PQXX_CHECK(
    (range{ibound{0}, ibound{inf}}).contains(inf),
    "Range to inclusive infinity does not include infinity.");
  PQXX_CHECK(
    not(range{ibound{0}, xbound{inf}}.contains(inf)),
    "Range to exclusive infinity includes infinity.");
  PQXX_CHECK(
    (range{ibound{0}, ubound{}}).contains(inf),
    "Right-unlimited range does not include infinity.");

  PQXX_CHECK(
    (range{ibound{-inf}, ibound{0}}).contains(-9999.0),
    "Range from infinity did not include large negative number.");
  PQXX_CHECK(
    not(range{ibound{-inf}, ibound{0}}.contains(0.1)),
    "Range from infinity includes number outside it.");
  PQXX_CHECK(
    (range{xbound{-inf}, ibound{0}}).contains(-9999.0),
    "Range from exclusive infinity did not include large negative number.");
  PQXX_CHECK(
    (range{ibound{-inf}, ibound{0}}).contains(-inf),
    "Range from inclusive infinity does not include negative infinity.");
  PQXX_CHECK(
    not(range{xbound{-inf}, ibound{0}}).contains(-inf),
    "Range to infinity exclusive includes negative infinity.");
  PQXX_CHECK(
    (range{ubound{}, ibound{0}}).contains(-inf),
    "Left-unlimited range does not include negative infinity.");
}


void test_range_subset()
{
  using range = pqxx::range<int>;
  using traits = pqxx::string_traits<range>;

  std::string_view subsets[][2]{
    {"empty", "empty"},  {"(,)", "empty"},    {"(0,1)", "empty"},
    {"(,)", "[-10,10]"}, {"(,)", "(-10,10)"}, {"(,)", "(,)"},
    {"(,10)", "(,10)"},  {"(,10)", "(,9)"},   {"(,10]", "(,10)"},
    {"(,10]", "(,10]"},  {"(1,)", "(10,)"},   {"(1,)", "(9,)"},
    {"[1,)", "(10,)"},   {"[1,)", "[10,)"},   {"[0,5]", "[1,4]"},
    {"(0,5)", "[1,4]"},
  };
  for (auto const [super, sub] : subsets)
    PQXX_CHECK(
      traits::from_string(super).contains(traits::from_string(sub)),
      pqxx::internal::concat(
        "Range '", super, "' did not contain '", sub, "'."));

  std::string_view non_subsets[][2]{
    {"empty", "[0,0]"},   {"empty", "(,)"},     {"[-10,10]", "(,)"},
    {"(-10,10)", "(,)"},  {"(,9)", "(,10)"},    {"(,10)", "(,10]"},
    {"[1,4]", "[0,4]"},   {"[1,4]", "[1,5]"},   {"(0,10)", "[0,10]"},
    {"(0,10)", "(0,10]"}, {"(0,10)", "[0,10)"},
  };
  for (auto const [super, sub] : non_subsets)
    PQXX_CHECK(
      not traits::from_string(super).contains(traits::from_string(sub)),
      pqxx::internal::concat("Range '", super, "' contained '", sub, "'."));
}


void test_range_to_string()
{
  using range = pqxx::range<int>;
  using ibound = pqxx::inclusive_bound<int>;
  using xbound = pqxx::exclusive_bound<int>;
  using ubound = pqxx::no_bound;

  PQXX_CHECK_EQUAL(
    pqxx::to_string(range{}), "empty", "Empty range came out wrong.");

  PQXX_CHECK_EQUAL(
    pqxx::to_string(range{ibound{5}, ibound{8}}), "[5,8]",
    "Inclusive range came out wrong.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(range{xbound{5}, ibound{8}}), "(5,8]",
    "Left-exclusive range came out wrong.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(range{ibound{5}, xbound{8}}), "[5,8)",
    "Right-exclusive range came out wrong.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(range{xbound{5}, xbound{8}}), "(5,8)",
    "Exclusive range came out wrong.");

  // Unlimited boundaries can use brackets or parentheses.  Doesn't matter.
  // We cheat and use some white-box knowledge of our implementation here.
  PQXX_CHECK_EQUAL(
    pqxx::to_string(range{ubound{}, ubound{}}), "(,)",
    "Universal range came out unexpected.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(range{ubound{}, ibound{8}}), "(,8]",
    "Left-unlimited range came out unexpected.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(range{ubound{}, xbound{8}}), "(,8)",
    "Left-unlimited range came out unexpected.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(range{ibound{5}, ubound{}}), "[5,)",
    "Right-unlimited range came out unexpected.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(range{xbound{5}, ubound{}}), "(5,)",
    "Right-unlimited range came out unexpected.");
}


void test_parse_range()
{
  using range = pqxx::range<int>;
  using ubound = pqxx::no_bound;
  using traits = pqxx::string_traits<range>;

  constexpr std::string_view empties[]{"empty", "EMPTY", "eMpTy"};
  for (auto empty : empties)
    PQXX_CHECK(
      traits::from_string(empty).empty(),
      pqxx::internal::concat(
        "This was supposed to produce an empty range: '", empty, "'"));

  constexpr std::string_view universals[]{"(,)", "[,)", "(,]", "[,]"};
  for (auto univ : universals)
    PQXX_CHECK_EQUAL(
      traits::from_string(univ), (range{ubound{}, ubound{}}),
      pqxx::internal::concat(
        "This was supposed to produce a universal range: '", univ, "'"));

  PQXX_CHECK(
    traits::from_string("(0,10]").lower_bound().is_exclusive(),
    "Exclusive lower bound did not parse right.");
  PQXX_CHECK(
    traits::from_string("[0,10]").lower_bound().is_inclusive(),
    "Inclusive lower bound did not parse right.");
  PQXX_CHECK(
    traits::from_string("(0,10)").upper_bound().is_exclusive(),
    "Exclusive upper bound did not parse right.");
  PQXX_CHECK(
    traits::from_string("[0,10]").upper_bound().is_inclusive(),
    "Inclusive upper bound did not parse right.");

  PQXX_CHECK_EQUAL(
    *traits::from_string("(\"0\",\"10\")").lower_bound().value(), 0,
    "Quoted range boundary did not parse right.");
  PQXX_CHECK_EQUAL(
    *traits::from_string("(\"0\",\"10\")").upper_bound().value(), 10,
    "Quoted upper boundary did not parse right.");

  auto floats{
    pqxx::string_traits<pqxx::range<double>>::from_string("(0,1.0)")};
  PQXX_CHECK_GREATER(
    *floats.lower_bound().value(), -0.001,
    "Float lower bound is out of range.");
  PQXX_CHECK_LESS(
    *floats.lower_bound().value(), 0.001,
    "Float lower bound is out of range.");
  PQXX_CHECK_GREATER(
    *floats.upper_bound().value(), 0.999,
    "Float upper bound is out of range.");
  PQXX_CHECK_LESS(
    *floats.upper_bound().value(), 1.001,
    "Float upper bound is out of range.");
}


void test_parse_bad_range()
{
  using range = pqxx::range<int>;
  using conv_err = pqxx::conversion_error;
  using traits = pqxx::string_traits<range>;
  constexpr std::string_view bad_ranges[]{
    "",   "x",       "e",          "empt",       "emptyy",   "()",
    "[]", "(empty)", "(empty, 0)", "(0, empty)", ",",        "(,",
    ",)", "(1,2,3)", "(4,5x)",     "(null, 0)",  "[0, 1.0]", "[1.0, 0]",
  };

  for (auto bad : bad_ranges)
    PQXX_CHECK_THROWS(
      pqxx::ignore_unused(traits::from_string(bad)), conv_err,
      pqxx::internal::concat(
        "This range wasn't supposed to parse: '", bad, "'"));
}


/// Parse ranges lhs and rhs, return their intersection as a string.
template<typename TYPE>
std::string intersect(std::string_view lhs, std::string_view rhs)
{
  using traits = pqxx::string_traits<pqxx::range<TYPE>>;
  return pqxx::to_string(traits::from_string(lhs) & traits::from_string(rhs));
}


void test_range_intersection()
{
  // Intersections and their expected results, in text form.
  // Each row contains two ranges, and their intersection.
  std::string_view intersections[][3]{
    {"empty", "empty", "empty"},
    {"(,)", "empty", "empty"},
    {"[,]", "empty", "empty"},
    {"empty", "[0,10]", "empty"},
    {"(,)", "(,)", "(,)"},
    {"(,)", "(5,8)", "(5,8)"},
    {"(,)", "[5,8)", "[5,8)"},
    {"(,)", "(5,8]", "(5,8]"},
    {"(,)", "[5,8]", "[5,8]"},
    {"(-1000,10)", "(0,1000)", "(0,10)"},
    {"[-1000,10)", "(0,1000)", "(0,10)"},
    {"(-1000,10]", "(0,1000)", "(0,10]"},
    {"[-1000,10]", "(0,1000)", "(0,10]"},
    {"[0,100]", "[0,100]", "[0,100]"},
    {"[0,100]", "[0,100)", "[0,100)"},
    {"[0,100]", "(0,100]", "(0,100]"},
    {"[0,100]", "(0,100)", "(0,100)"},
    {"[0,10]", "[11,20]", "empty"},
    {"[0,10]", "(11,20]", "empty"},
    {"[0,10]", "[11,20)", "empty"},
    {"[0,10]", "(11,20)", "empty"},
    {"[0,10]", "[10,11]", "[10,10]"},
    {"[0,10)", "[10,11]", "empty"},
    {"[0,10]", "(10,11]", "empty"},
    {"[0,10)", "(10,11]", "empty"},
  };
  for (auto [left, right, expected] : intersections)
  {
    PQXX_CHECK_EQUAL(
      intersect<int>(left, right), expected,
      pqxx::internal::concat(
        "Intersection of '", left, "' and '", right,
        " produced unexpected result."));
    PQXX_CHECK_EQUAL(
      intersect<int>(right, left), expected,
      pqxx::internal::concat(
        "Intersection of '", left, "' and '", right, " was asymmetric."));
  }
}


void test_range_conversion()
{
  std::string_view const ranges[]{
    "empty", "(,)", "(,10)", "(0,)", "[0,10]", "[0,10)", "(0,10]", "(0,10)",
  };

  for (auto r : ranges)
  {
    auto const shortr{pqxx::from_string<pqxx::range<short>>(r)};
    pqxx::range<int> intr{shortr};
    PQXX_CHECK_EQUAL(
      pqxx::to_string(intr), r, "Converted range looks different.");
  }
}


PQXX_REGISTER_TEST(test_range_construct);
PQXX_REGISTER_TEST(test_range_equality);
PQXX_REGISTER_TEST(test_range_empty);
PQXX_REGISTER_TEST(test_range_contains);
PQXX_REGISTER_TEST(test_float_range_contains);
PQXX_REGISTER_TEST(test_range_subset);
PQXX_REGISTER_TEST(test_range_to_string);
PQXX_REGISTER_TEST(test_parse_range);
PQXX_REGISTER_TEST(test_parse_bad_range);
PQXX_REGISTER_TEST(test_range_intersection);
PQXX_REGISTER_TEST(test_range_conversion);
} // namespace
