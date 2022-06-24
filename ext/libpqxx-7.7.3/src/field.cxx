/** Implementation of the pqxx::field class.
 *
 * pqxx::field refers to a field in a query result.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include <cstring>

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/field.hxx"
#include "pqxx/internal/libpq-forward.hxx"
#include "pqxx/result.hxx"
#include "pqxx/row.hxx"

#include "pqxx/internal/header-post.hxx"


pqxx::field::field(pqxx::row const &r, pqxx::row::size_type c) noexcept :
        m_col{c}, m_home{r.m_result}, m_row{r.m_index}
{}


bool PQXX_COLD pqxx::field::operator==(field const &rhs) const
{
  if (is_null() and rhs.is_null())
    return true;
  if (is_null() != rhs.is_null())
    return false;
  auto const s{size()};
  return (s == std::size(rhs)) and (std::memcmp(c_str(), rhs.c_str(), s) == 0);
}


char const *pqxx::field::name() const &
{
  return home().column_name(col());
}


pqxx::oid pqxx::field::type() const
{
  return home().column_type(col());
}


pqxx::oid pqxx::field::table() const
{
  return home().column_table(col());
}


pqxx::row::size_type pqxx::field::table_column() const
{
  return home().table_column(col());
}


char const *pqxx::field::c_str() const &
{
  return home().get_value(idx(), col());
}


bool pqxx::field::is_null() const noexcept
{
  return home().get_is_null(idx(), col());
}


pqxx::field::size_type pqxx::field::size() const noexcept
{
  return home().get_length(idx(), col());
}
