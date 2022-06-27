#include <pqxx/transaction>

#include "../test_helpers.hxx"

// Test program for libpqxx array parsing.

namespace pqxx
{
template<>
struct nullness<array_parser::juncture> : no_null<array_parser::juncture>
{};


inline std::string to_string(pqxx::array_parser::juncture const &j)
{
  using junc = pqxx::array_parser::juncture;
  switch (j)
  {
  case junc::row_start: return "row_start";
  case junc::row_end: return "row_end";
  case junc::null_value: return "null_value";
  case junc::string_value: return "string_value";
  case junc::done: return "done";
  default: return "UNKNOWN JUNCTURE: " + to_string(static_cast<int>(j));
  }
}
} // namespace pqxx


namespace
{
void test_empty_arrays()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;

  // Parsing a null pointer just immediately returns "done".
  output = pqxx::array_parser(std::string_view()).get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "get_next on null array did not return done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  // Parsing an empty array string immediately returns "done".
  output = pqxx::array_parser("").get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "get_next on an empty array string did not return done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  // Parsing an empty array returns "row_start", "row_end", "done".
  pqxx::array_parser empty_parser("{}");
  output = empty_parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Empty array did not start with row_start.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = empty_parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Empty array did not end with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = empty_parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "Empty array did not conclude with done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");
}


void test_null_value()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;
  pqxx::array_parser containing_null("{NULL}");

  output = containing_null.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Array containing null did not start with row_start.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = containing_null.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::null_value,
    "Array containing null did not return null_value.");
  PQXX_CHECK_EQUAL(output.second, "", "Null value was not empty.");

  output = containing_null.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Array containing null did not end with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = containing_null.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "Array containing null did not conclude with done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");
}


void test_single_quoted_string()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;
  pqxx::array_parser parser("{'item'}");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Array did not start with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "item", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Array did not end with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "Array did not conclude with done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");
}


void test_single_quoted_escaping()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;
  pqxx::array_parser parser("{'don''t\\\\ care'}");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Array did not start with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "don't\\ care", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Array did not end with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "Array did not conclude with done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");
}


void test_double_quoted_string()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;
  pqxx::array_parser parser("{\"item\"}");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Array did not start with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "item", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Array did not end with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "Array did not conclude with done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");
}


void test_double_quoted_escaping()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;
  pqxx::array_parser parser(R"--({"don''t\\ care"})--");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Array did not start with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "don''t\\ care", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Array did not end with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "Array did not conclude with done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");
}


// A pair of double quotes in a double-quoted string is an escaped quote.
void test_double_double_quoted_string()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;
  pqxx::array_parser parser{R"--({"3"" steel"})--"};

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Array did not start with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");

  PQXX_CHECK_EQUAL(output.second, "3\" steel", "Unexpected string value.");
}


void test_unquoted_string()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;
  pqxx::array_parser parser("{item}");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Array did not start with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "item", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Array did not end with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "Array did not conclude with done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");
}


void test_multiple_values()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;
  pqxx::array_parser parser("{1,2}");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Array did not start with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "1", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "2", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Array did not end with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "Array did not conclude with done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");
}


void test_nested_array()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;
  pqxx::array_parser parser("{{item}}");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Array did not start with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Nested array did not start 2nd dimension with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "item", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Nested array did not end 2nd dimension with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Array did not end with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "Array did not conclude with done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");
}


void test_nested_array_with_multiple_entries()
{
  std::pair<pqxx::array_parser::juncture, std::string> output;
  pqxx::array_parser parser("{{1,2},{3,4}}");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Array did not start with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Nested array did not start 2nd dimension with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "1", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "2", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Nested array did not end 2nd dimension with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_start,
    "Nested array did not descend to 2nd dimension with row_start.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "3", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::string_value,
    "Array did not return string_value.");
  PQXX_CHECK_EQUAL(output.second, "4", "Unexpected string value.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Nested array did not leave 2nd dimension with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::row_end,
    "Array did not end with row_end.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");

  output = parser.get_next();
  PQXX_CHECK_EQUAL(
    output.first, pqxx::array_parser::juncture::done,
    "Array did not conclude with done.");
  PQXX_CHECK_EQUAL(output.second, "", "Unexpected nonempty output.");
}


void test_array_parse()
{
  test_empty_arrays();
  test_null_value();
  test_single_quoted_string();
  test_single_quoted_escaping();
  test_double_quoted_string();
  test_double_quoted_escaping();
  test_double_double_quoted_string();
  test_unquoted_string();
  test_multiple_values();
  test_nested_array();
  test_nested_array_with_multiple_entries();
}


void test_generate_empty_array()
{
  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::vector<int>{}), "{}",
    "Basic array output is not as expected.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::vector<std::string>{}), "{}",
    "String array comes out different.");
}


void test_generate_null_value()
{
  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::vector<char const *>{nullptr}), "{NULL}",
    "Null array value did not come out as expected.");
}


void test_generate_single_item()
{
  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::vector<int>{42}), "{42}",
    "Numeric conversion came out wrong.");

  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::vector<char const *>{"foo"}), "{\"foo\"}",
    "String array conversion came out wrong.");
}


void test_generate_multiple_items()
{
  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::vector<int>{5, 4, 3, 2}), "{5,4,3,2}",
    "Array with multiple values is not correct.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::vector<std::string>{"foo", "bar"}),
    "{\"foo\",\"bar\"}", "Array with multiple strings came out wrong.");
}


void test_generate_nested_array()
{
  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::vector<std::vector<int>>{{1, 2}, {3, 4}}),
    "{{1,2},{3,4}}", "Nested arrays don't work right.");
}


void test_generate_escaped_strings()
{
  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::vector<std::string>{"a\\b"}), "{\"a\\\\b\"}",
    "Backslashes are not escaped properly.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::vector<std::string>{"x\"y\""}), "{\"x\\\"y\\\"\"}",
    "Double quotes are not escaped properly.");
}


void test_array_generate()
{
  test_generate_empty_array();
  test_generate_null_value();
  test_generate_single_item();
  test_generate_multiple_items();
  test_generate_nested_array();
  test_generate_escaped_strings();
}


void test_array_roundtrip()
{
  pqxx::connection c;
  pqxx::work w{c};

  std::vector<int> const in{0, 1, 2, 3, 5};
  auto const r1{w.exec1("SELECT " + c.quote(in) + "::integer[]")};
  pqxx::array_parser parser{r1[0].view()};
  auto item{parser.get_next()};
  PQXX_CHECK_EQUAL(
    item.first, pqxx::array_parser::juncture::row_start,
    "Array did not start with row_start.");

  std::vector<int> out;
  for (item = parser.get_next();
       item.first == pqxx::array_parser::juncture::string_value;
       item = parser.get_next())
  {
    out.push_back(pqxx::from_string<int>(item.second));
  }

  PQXX_CHECK_EQUAL(
    item.first, pqxx::array_parser::juncture::row_end,
    "Array values did not end in row_end.");
  PQXX_CHECK_EQUAL(
    std::size(out), std::size(in), "Array came back with different length.");

  for (std::size_t i{0}; i < std::size(in); ++i)
    PQXX_CHECK_EQUAL(out[i], in[i], "Array element has changed.");

  item = parser.get_next();
  PQXX_CHECK_EQUAL(
    item.first, pqxx::array_parser::juncture::done,
    "Array did not end in done.");
}


PQXX_REGISTER_TEST(test_array_parse);
PQXX_REGISTER_TEST(test_array_generate);
PQXX_REGISTER_TEST(test_array_roundtrip);
} // namespace
