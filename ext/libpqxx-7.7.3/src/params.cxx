/* Implementations related to prepared and parameterised statements.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/params.hxx"

#include "pqxx/internal/header-post.hxx"


void pqxx::internal::c_params::reserve(std::size_t n) &
{
  values.reserve(n);
  lengths.reserve(n);
  formats.reserve(n);
}


void pqxx::params::reserve(std::size_t n) &
{
  m_params.reserve(n);
}


void pqxx::params::append() &
{
  m_params.emplace_back(nullptr);
}


void pqxx::params::append(zview value) &
{
  m_params.emplace_back(value);
}


void pqxx::params::append(std::string const &value) &
{
  m_params.emplace_back(value);
}


void pqxx::params::append(std::string &&value) &
{
  m_params.emplace_back(std::move(value));
}


void pqxx::params::append(params const &value) &
{
  this->reserve(std::size(value.m_params) + std::size(this->m_params));
  for (auto const &param : value.m_params) m_params.emplace_back(param);
}


void pqxx::params::append(std::basic_string_view<std::byte> value) &
{
  m_params.emplace_back(value);
}


void pqxx::params::append(std::basic_string<std::byte> const &value) &
{
  m_params.emplace_back(value);
}


void pqxx::params::append(std::basic_string<std::byte> &&value) &
{
  m_params.emplace_back(std::move(value));
}


void PQXX_COLD pqxx::params::append(binarystring const &value) &
{
  m_params.push_back(entry{value.bytes_view()});
}


void pqxx::params::append(params &&value) &
{
  this->reserve(std::size(value.m_params) + std::size(this->m_params));
  for (auto const &param : value.m_params)
    m_params.emplace_back(std::move(param));
  value.m_params.clear();
}


pqxx::internal::c_params pqxx::params::make_c_params() const
{
  pqxx::internal::c_params p;
  p.reserve(std::size(m_params));
  for (auto const &param : m_params)
    std::visit(
      [&p](auto const &value) {
        using T = strip_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::nullptr_t>)
        {
          p.values.push_back(nullptr);
          p.lengths.push_back(0);
        }
        else
        {
          p.values.push_back(reinterpret_cast<char const *>(std::data(value)));
          p.lengths.push_back(
            check_cast<int>(internal::ssize(value), s_overflow));
        }

        p.formats.push_back(param_format(value));
      },
      param);

  return p;
}
