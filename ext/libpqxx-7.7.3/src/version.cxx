/** Version check.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/version.hxx"

#include "pqxx/internal/header-post.hxx"


namespace pqxx::internal
{
// One, single definition of this function.  If a call fails to link, and
// (some) other calls do link, then the libpqxx binary was built against a
// different libpqxx version than the code which is being linked against it.
PQXX_LIBEXPORT int PQXX_VERSION_CHECK() noexcept
{
  return 0;
}
} // namespace pqxx::internal
