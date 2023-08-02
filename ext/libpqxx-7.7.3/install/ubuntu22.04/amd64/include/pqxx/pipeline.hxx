/* Definition of the pqxx::pipeline class.
 *
 *   Throughput-optimized mechanism for executing queries.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/pipeline instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_PIPELINE
#define PQXX_H_PIPELINE

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <limits>
#include <map>
#include <string>

#include "pqxx/transaction_base.hxx"


namespace pqxx
{
// TODO: libpq 14 introduced a similar "pipeline mode."  Can we use that?

/// Processes several queries in FIFO manner, optimized for high throughput.
/** Use a pipeline if you want to keep doing useful work while your queries are
 * executing.  Result retrieval is decoupled from execution request; queries
 * "go in at the front" and results "come out the back."
 *
 * Actually, you can retrieve the results in any order if you want, but it may
 * lead to surprising "time travel" effects if any of the queries fails.  In
 * particular, syntax errors in the queries can confuse things and show up too
 * early in the stream of results.
 *
 * Generally, if any of the queries fails, it will throw an exception at the
 * point where you request its result.  But it may happen earlier, especially
 * if you request results out of chronological order.
 *
 * @warning While a pipeline is active, you cannot execute queries, open
 * streams, etc. on the same transaction.  A transaction can have at most one
 * object of a type derived from @ref pqxx::transaction_focus active on it at a
 * time.
 */
class PQXX_LIBEXPORT pipeline : public transaction_focus
{
public:
  /// Identifying numbers for queries.
  using query_id = long;

  pipeline(pipeline const &) = delete;
  pipeline &operator=(pipeline const &) = delete;

  /// Start a pipeline.
  explicit pipeline(transaction_base &t) : transaction_focus{t, s_classname}
  {
    init();
  }
  /// Start a pipeline.  Assign it a name, for more helpful error messages.
  pipeline(transaction_base &t, std::string_view tname) :
          transaction_focus{t, s_classname, tname}
  {
    init();
  }

  /// Close the pipeline.
  ~pipeline() noexcept;

  /// Add query to the pipeline.
  /** Queries accumulate in the pipeline, which sends them to the backend in a
   * batch separated by semicolons.  The queries you insert must not use this
   * trick themselves, or the pipeline will get hopelessly confused!
   *
   * @return Identifier for this query, unique only within this pipeline.
   */
  query_id insert(std::string_view) &;

  /// Wait for all ongoing or pending operations to complete, and detach.
  /** Detaches from the transaction when done.
   *
   * This does not produce the queries' results, so it may not report any
   * errors which may have occurred in their execution.  To be sure that your
   * statements succeeded, call @ref retrieve until the pipeline is empty.
   */
  void complete();

  /// Forget all ongoing or pending operations and retrieved results.
  /** Queries already sent to the backend may still be completed, depending
   * on implementation and timing.
   *
   * Any error state (unless caused by an internal error) will also be cleared.
   * This is mostly useful in a nontransaction, since a backend transaction is
   * aborted automatically when an error occurs.
   *
   * Detaches from the transaction when done.
   */
  void flush();

  /// Cancel ongoing query, if any.
  /** May cancel any or all of the queries that have been inserted at this
   * point whose results have not yet been retrieved.  If the pipeline lives in
   * a backend transaction, that transaction may be left in a nonfunctional
   * state in which it can only be aborted.
   *
   * Therefore, either use this function in a nontransaction, or abort the
   * transaction after calling it.
   */
  void cancel();

  /// Is result for given query available?
  [[nodiscard]] bool is_finished(query_id) const;

  /// Retrieve result for given query.
  /** If the query failed for whatever reason, this will throw an exception.
   * The function will block if the query has not finished yet.
   * @warning If results are retrieved out-of-order, i.e. in a different order
   * than the one in which their queries were inserted, errors may "propagate"
   * to subsequent queries.
   */
  result retrieve(query_id qid)
  {
    return retrieve(m_queries.find(qid)).second;
  }

  /// Retrieve oldest unretrieved result (possibly wait for one).
  /** @return The query's identifier and its result set. */
  std::pair<query_id, result> retrieve();

  [[nodiscard]] bool empty() const noexcept { return std::empty(m_queries); }

  /// Set maximum number of queries to retain before issuing them to the
  /// backend.
  /** The pipeline will perform better if multiple queries are issued at once,
   * but retaining queries until the results are needed (as opposed to issuing
   * them to the backend immediately) may negate any performance benefits the
   * pipeline can offer.
   *
   * Recommended practice is to set this value no higher than the number of
   * queries you intend to insert at a time.
   * @param retain_max A nonnegative "retention capacity;" passing zero will
   * cause queries to be issued immediately
   * @return Old retention capacity
   */
  int retain(int retain_max = 2) &;


  /// Resume retained query emission.  Harmless when not needed.
  void resume() &;

private:
  struct PQXX_PRIVATE Query
  {
    explicit Query(std::string_view q) :
            query{std::make_shared<std::string>(q)}
    {}

    std::shared_ptr<std::string> query;
    result res;
  };

  using QueryMap = std::map<query_id, Query>;

  void init();
  void attach();
  void detach();

  /// Upper bound to query id's.
  static constexpr query_id qid_limit() noexcept
  {
    // Parenthesise this to work around an eternal Visual C++ problem:
    // Without the extra parentheses, unless NOMINMAX is defined, the
    // preprocessor will mistake this "max" for its annoying built-in macro
    // of the same name.
    return (std::numeric_limits<query_id>::max)();
  }

  /// Create new query_id.
  PQXX_PRIVATE query_id generate_id();

  bool have_pending() const noexcept
  {
    return m_issuedrange.second != m_issuedrange.first;
  }

  PQXX_PRIVATE void issue();

  /// The given query failed; never issue anything beyond that.
  void set_error_at(query_id qid) noexcept
  {
    PQXX_UNLIKELY
    if (qid < m_error)
      m_error = qid;
  }

  /// Throw pqxx::internal_error.
  [[noreturn]] PQXX_PRIVATE void internal_error(std::string const &err);

  PQXX_PRIVATE bool obtain_result(bool expect_none = false);

  PQXX_PRIVATE void obtain_dummy();
  PQXX_PRIVATE void get_further_available_results();
  PQXX_PRIVATE void check_end_results();

  /// Receive any results that happen to be available; it's not urgent.
  PQXX_PRIVATE void receive_if_available();

  /// Receive results, up to stop if possible.
  PQXX_PRIVATE void receive(pipeline::QueryMap::const_iterator stop);
  std::pair<pipeline::query_id, result> retrieve(pipeline::QueryMap::iterator);

  QueryMap m_queries;
  std::pair<QueryMap::iterator, QueryMap::iterator> m_issuedrange;
  int m_retain = 0;
  int m_num_waiting = 0;
  query_id m_q_id = 0;

  /// Is there a "dummy query" pending?
  bool m_dummy_pending = false;

  /// Point at which an error occurred; no results beyond it will be available
  query_id m_error = qid_limit();

  /// Encoding.
  /** We store this in the object to avoid the risk of exceptions at awkward
   * moments.
   */
  internal::encoding_group m_encoding;

  static constexpr std::string_view s_classname{"pipeline"};
};
} // namespace pqxx
#endif
