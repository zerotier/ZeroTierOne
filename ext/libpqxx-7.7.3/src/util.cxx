/** Various utility functions.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>

extern "C"
{
#include <libpq-fe.h>
}

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/except.hxx"
#include "pqxx/internal/concat.hxx"
#include "pqxx/util.hxx"

#include "pqxx/internal/header-post.hxx"


using namespace std::literals;

pqxx::thread_safety_model PQXX_COLD pqxx::describe_thread_safety()
{
  thread_safety_model model;
  model.safe_libpq = (PQisthreadsafe() != 0);
  // Sadly I'm not aware of any way to avoid this just yet.
  model.safe_kerberos = false;

  model.description = internal::concat(
    (model.safe_libpq ? ""sv :
                        "Using a libpq build that is not thread-safe.\n"sv),
    (model.safe_kerberos ?
       ""sv :
       "Kerberos is not thread-safe.  If your application uses Kerberos, "
       "protect all calls to Kerberos or libpqxx using a global lock.\n"sv));
  return model;
}


std::string pqxx::internal::describe_object(
  std::string_view class_name, std::string_view obj_name)
{
  if (std::empty(obj_name))
    return std::string{class_name};
  else
    return pqxx::internal::concat(class_name, " '", obj_name, "'");
}


void pqxx::internal::check_unique_register(
  void const *old_guest, std::string_view old_class, std::string_view old_name,
  void const *new_guest, std::string_view new_class, std::string_view new_name)
{
  if (new_guest == nullptr)
    throw internal_error{"Null pointer registered."};

  if (old_guest != nullptr)
    throw usage_error{
      (old_guest == new_guest) ?
        concat("Started twice: ", describe_object(old_class, old_name), ".") :
        concat(
          "Started new ", describe_object(new_class, new_name), " while ",
          describe_object(new_class, new_name), " was still active.")};
}


void pqxx::internal::check_unique_unregister(
  void const *old_guest, std::string_view old_class, std::string_view old_name,
  void const *new_guest, std::string_view new_class, std::string_view new_name)
{
  if (new_guest != old_guest)
  {
    PQXX_UNLIKELY
    if (new_guest == nullptr)
      throw usage_error{concat(
        "Expected to close ", describe_object(old_class, old_name),
        ", but got null pointer instead.")};
    if (old_guest == nullptr)
      throw usage_error{concat(
        "Closed while not open: ", describe_object(new_class, new_name))};
    else
      throw usage_error{concat(
        "Closed ", describe_object(new_class, new_name),
        "; expected to close ", describe_object(old_class, old_name))};
  }
}


namespace
{
constexpr char hex_digits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};


/// Translate a number (must be between 0 and 16 exclusive) to a hex digit.
constexpr char hex_digit(int c) noexcept
{
  return hex_digits[c];
}


/// Translate a hex digit to a nibble.  Return -1 if it's not a valid digit.
constexpr int nibble(int c) noexcept
{
  if (c >= '0' and c <= '9')
    PQXX_LIKELY
  return c - '0';
  else if (c >= 'a' and c <= 'f') return 10 + (c - 'a');
  else if (c >= 'A' and c <= 'F') return 10 + (c - 'A');
  else return -1;
}
} // namespace


void pqxx::internal::esc_bin(
  std::basic_string_view<std::byte> binary_data, char buffer[]) noexcept
{
  auto here{buffer};
  *here++ = '\\';
  *here++ = 'x';

  for (auto const byte : binary_data)
  {
    auto uc{static_cast<unsigned char>(byte)};
    *here++ = hex_digit(uc >> 4);
    *here++ = hex_digit(uc & 0x0f);
  }

  // (No need to increment further.  Facebook's "infer" complains if we do.)
  *here = '\0';
}


std::string
pqxx::internal::esc_bin(std::basic_string_view<std::byte> binary_data)
{
  auto const bytes{size_esc_bin(std::size(binary_data))};
  std::string buf;
  buf.resize(bytes);
  esc_bin(binary_data, buf.data());
  // Strip off the trailing zero.
  buf.resize(bytes - 1);
  return buf;
}


void pqxx::internal::unesc_bin(
  std::string_view escaped_data, std::byte buffer[])
{
  auto const in_size{std::size(escaped_data)};
  if (in_size < 2)
    throw pqxx::failure{"Binary data appears truncated."};
  if ((in_size % 2) != 0)
    throw pqxx::failure{"Invalid escaped binary length."};
  char const *in{escaped_data.data()};
  char const *const end{in + in_size};
  if (*in++ != '\\' or *in++ != 'x')
    throw pqxx::failure(
      "Escaped binary data did not start with '\\x'`.  Is the server or libpq "
      "too old?");
  auto out{buffer};
  while (in != end)
  {
    int hi{nibble(*in++)};
    if (hi < 0)
      throw pqxx::failure{"Invalid hex-escaped data."};
    int lo{nibble(*in++)};
    if (lo < 0)
      throw pqxx::failure{"Invalid hex-escaped data."};
    *out++ = static_cast<std::byte>((hi << 4) | lo);
  }
}


std::basic_string<std::byte>
pqxx::internal::unesc_bin(std::string_view escaped_data)
{
  auto const bytes{size_unesc_bin(std::size(escaped_data))};
  std::basic_string<std::byte> buf;
  buf.resize(bytes);
  unesc_bin(escaped_data, buf.data());
  return buf;
}
