/** Implementation of the pqxx::pipeline class.
 *
 * Throughput-optimized query interface.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include <iterator>

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/dbtransaction.hxx"
#include "pqxx/internal/concat.hxx"
#include "pqxx/internal/gates/connection-pipeline.hxx"
#include "pqxx/internal/gates/result-creation.hxx"
#include "pqxx/internal/gates/result-pipeline.hxx"
#include "pqxx/pipeline.hxx"
#include "pqxx/separated_list.hxx"

#include "pqxx/internal/header-post.hxx"


namespace
{
std::string const theSeparator{"; "};
std::string const theDummyValue{"1"};
std::string const theDummyQuery{"SELECT " + theDummyValue + theSeparator};
} // namespace


void pqxx::pipeline::init()
{
  m_encoding = internal::enc_group(m_trans.conn().encoding_id());
  m_issuedrange = make_pair(std::end(m_queries), std::end(m_queries));
  attach();
}


pqxx::pipeline::~pipeline() noexcept
{
  try
  {
    cancel();
  }
  catch (std::exception const &)
  {}
  detach();
}


void pqxx::pipeline::attach()
{
  if (not registered())
    register_me();
}


void pqxx::pipeline::detach()
{
  if (registered())
    unregister_me();
}


pqxx::pipeline::query_id pqxx::pipeline::insert(std::string_view q) &
{
  attach();
  query_id const qid{generate_id()};
  auto const i{m_queries.insert(std::make_pair(qid, Query(q))).first};

  if (m_issuedrange.second == std::end(m_queries))
  {
    m_issuedrange.second = i;
    if (m_issuedrange.first == std::end(m_queries))
      m_issuedrange.first = i;
  }
  m_num_waiting++;

  if (m_num_waiting > m_retain)
  {
    if (have_pending())
      receive_if_available();
    if (not have_pending())
      issue();
  }

  return qid;
}


void pqxx::pipeline::complete()
{
  if (have_pending())
    receive(m_issuedrange.second);
  if (m_num_waiting and (m_error == qid_limit()))
  {
    issue();
    receive(std::end(m_queries));
  }
  detach();
}


void pqxx::pipeline::flush()
{
  if (not std::empty(m_queries))
  {
    if (have_pending())
      receive(m_issuedrange.second);
    m_issuedrange.first = m_issuedrange.second = std::end(m_queries);
    m_num_waiting = 0;
    m_dummy_pending = false;
    m_queries.clear();
  }
  detach();
}


void PQXX_COLD pqxx::pipeline::cancel()
{
  while (have_pending())
  {
    pqxx::internal::gate::connection_pipeline(m_trans.conn()).cancel_query();
    auto canceled_query{m_issuedrange.first};
    ++m_issuedrange.first;
    m_queries.erase(canceled_query);
  }
}


bool pqxx::pipeline::is_finished(pipeline::query_id q) const
{
  if (m_queries.find(q) == std::end(m_queries))
    throw std::logic_error{
      internal::concat("Requested status for unknown query '", q, "'.")};
  return (QueryMap::const_iterator(m_issuedrange.first) ==
          std::end(m_queries)) or
         (q < m_issuedrange.first->first and q < m_error);
}


std::pair<pqxx::pipeline::query_id, pqxx::result> pqxx::pipeline::retrieve()
{
  if (std::empty(m_queries))
    throw std::logic_error{"Attempt to retrieve result from empty pipeline."};
  return retrieve(std::begin(m_queries));
}


int pqxx::pipeline::retain(int retain_max) &
{
  if (retain_max < 0)
    throw range_error{internal::concat(
      "Attempt to make pipeline retain ", retain_max, " queries")};

  int const oldvalue{m_retain};
  m_retain = retain_max;

  if (m_num_waiting >= m_retain)
    resume();

  return oldvalue;
}


void pqxx::pipeline::resume() &
{
  if (have_pending())
    receive_if_available();
  if (not have_pending() and m_num_waiting)
  {
    issue();
    receive_if_available();
  }
}


pqxx::pipeline::query_id pqxx::pipeline::generate_id()
{
  if (m_q_id == qid_limit())
    throw std::overflow_error{"Too many queries went through pipeline."};
  ++m_q_id;
  return m_q_id;
}


void pqxx::pipeline::issue()
{
  // Retrieve that null result for the last query, if needed.
  obtain_result();

  // Don't issue anything if we've encountered an error.
  if (m_error < qid_limit())
    return;

  // Start with oldest query (lowest id) not in previous issue range.
  auto oldest{m_issuedrange.second};

  // Construct cumulative query string for entire batch.
  auto cum{separated_list(
    theSeparator, oldest, std::end(m_queries),
    [](QueryMap::const_iterator i) { return i->second.query; })};
  auto const num_issued{
    QueryMap::size_type(std::distance(oldest, std::end(m_queries)))};
  bool const prepend_dummy{num_issued > 1};
  if (prepend_dummy)
    cum = theDummyQuery + cum;

  pqxx::internal::gate::connection_pipeline{m_trans.conn()}.start_exec(
    cum.c_str());

  // Since we managed to send out these queries, update state to reflect this.
  m_dummy_pending = prepend_dummy;
  m_issuedrange.first = oldest;
  m_issuedrange.second = std::end(m_queries);
  m_num_waiting -= check_cast<int>(num_issued, "pipeline issue()"sv);
}


void PQXX_COLD pqxx::pipeline::internal_error(std::string const &err)
{
  set_error_at(0);
  throw pqxx::internal_error{err};
}


bool pqxx::pipeline::obtain_result(bool expect_none)
{
  pqxx::internal::gate::connection_pipeline gate{m_trans.conn()};
  auto const r{gate.get_result()};
  if (r == nullptr)
  {
    if (have_pending() and not expect_none)
    {
      PQXX_UNLIKELY
      set_error_at(m_issuedrange.first->first);
      m_issuedrange.second = m_issuedrange.first;
    }
    return false;
  }

  result const res{pqxx::internal::gate::result_creation::create(
    r, std::begin(m_queries)->second.query, m_encoding)};

  if (not have_pending())
  {
    PQXX_UNLIKELY
    set_error_at(std::begin(m_queries)->first);
    throw std::logic_error{
      "Got more results from pipeline than there were queries."};
  }

  // Must be the result for the oldest pending query.
  if (not std::empty(m_issuedrange.first->second.res))
    PQXX_UNLIKELY
  internal_error("Multiple results for one query.");

  m_issuedrange.first->second.res = res;
  ++m_issuedrange.first;

  return true;
}


void pqxx::pipeline::obtain_dummy()
{
  // Allocate once, re-use across invocations.
  static auto const text{
    std::make_shared<std::string>("[DUMMY PIPELINE QUERY]")};

  pqxx::internal::gate::connection_pipeline gate{m_trans.conn()};
  auto const r{gate.get_result()};
  m_dummy_pending = false;

  if (r == nullptr)
    PQXX_UNLIKELY
  internal_error("Pipeline got no result from backend when it expected one.");

  result R{pqxx::internal::gate::result_creation::create(r, text, m_encoding)};

  bool OK{false};
  try
  {
    pqxx::internal::gate::result_creation{R}.check_status();
    OK = true;
  }
  catch (sql_error const &)
  {}
  if (OK)
  {
    PQXX_LIKELY
    if (std::size(R) > 1)
      PQXX_UNLIKELY
    internal_error("Unexpected result for dummy query in pipeline.");

    if (R.at(0).at(0).as<std::string>() != theDummyValue)
      PQXX_UNLIKELY
    internal_error("Dummy query in pipeline returned unexpected value.");
    return;
  }

  // TODO: Can we actually re-issue statements after a failure?
  /* Execution of this batch failed.
   *
   * When we send multiple statements in one go, the backend treats them as a
   * single transaction.  So the entire batch was effectively rolled back.
   *
   * Since none of the queries in the batch were actually executed, we can
   * afford to replay them one by one until we find the exact query that
   * caused the error.  This gives us not only a more specific error message
   * to report, but also tells us which query to report it for.
   */
  // First, give the whole batch the same syntax error message, in case all
  // else is going to fail.
  for (auto i{m_issuedrange.first}; i != m_issuedrange.second; ++i)
    i->second.res = R;

  // Remember where the end of this batch was
  auto const stop{m_issuedrange.second};

  // Retrieve that null result for the last query, if needed
  obtain_result(true);

  // Reset internal state to forget botched batch attempt
  m_num_waiting += check_cast<int>(
    std::distance(m_issuedrange.first, stop), "pipeline obtain_dummy()"sv);
  m_issuedrange.second = m_issuedrange.first;

  // Issue queries in failed batch one at a time.
  unregister_me();
  try
  {
    do {
      m_num_waiting--;
      auto const query{*m_issuedrange.first->second.query};
      auto &holder{m_issuedrange.first->second};
      holder.res = m_trans.exec(query);
      pqxx::internal::gate::result_creation{holder.res}.check_status();
      ++m_issuedrange.first;
    } while (m_issuedrange.first != stop);
  }
  catch (std::exception const &)
  {
    auto const thud{m_issuedrange.first->first};
    ++m_issuedrange.first;
    m_issuedrange.second = m_issuedrange.first;
    auto q{m_issuedrange.first};
    set_error_at((q == std::end(m_queries)) ? thud + 1 : q->first);
  }
}


std::pair<pqxx::pipeline::query_id, pqxx::result>
pqxx::pipeline::retrieve(pipeline::QueryMap::iterator q)
{
  if (q == std::end(m_queries))
    throw std::logic_error{"Attempt to retrieve result for unknown query."};

  if (q->first >= m_error)
    throw std::runtime_error{
      "Could not complete query in pipeline due to error in earlier query."};

  // If query hasn't issued yet, do it now.
  if (
    m_issuedrange.second != std::end(m_queries) and
    (q->first >= m_issuedrange.second->first))
  {
    if (have_pending())
      receive(m_issuedrange.second);
    if (m_error == qid_limit())
      issue();
  }

  // If result not in yet, get it; else get at least whatever's convenient.
  if (have_pending())
  {
    if (q->first >= m_issuedrange.first->first)
    {
      auto suc{q};
      ++suc;
      receive(suc);
    }
    else
    {
      receive_if_available();
    }
  }

  if (q->first >= m_error)
    throw std::runtime_error{
      "Could not complete query in pipeline due to error in earlier query."};

  // Don't leave the backend idle if there are queries waiting to be issued.
  if (m_num_waiting and not have_pending() and (m_error == qid_limit()))
    issue();

  result const R{q->second.res};
  auto const P{std::make_pair(q->first, R)};

  m_queries.erase(q);

  pqxx::internal::gate::result_creation{R}.check_status();
  return P;
}


void pqxx::pipeline::get_further_available_results()
{
  pqxx::internal::gate::connection_pipeline gate{m_trans.conn()};
  while (not gate.is_busy() and obtain_result())
    if (not gate.consume_input())
      throw broken_connection{};
}


void pqxx::pipeline::receive_if_available()
{
  pqxx::internal::gate::connection_pipeline gate{m_trans.conn()};
  if (not gate.consume_input())
    throw broken_connection{};
  if (gate.is_busy())
    return;

  if (m_dummy_pending)
    obtain_dummy();
  if (have_pending())
    get_further_available_results();
}


void pqxx::pipeline::receive(pipeline::QueryMap::const_iterator stop)
{
  if (m_dummy_pending)
    obtain_dummy();

  while (obtain_result() and
         QueryMap::const_iterator{m_issuedrange.first} != stop)
    ;

  // Also haul in any remaining "targets of opportunity".
  if (QueryMap::const_iterator{m_issuedrange.first} == stop)
    get_further_available_results();
}
