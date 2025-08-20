/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DO NOT EDIT, this is an Auto-generated file from:
 * buildscripts/semantic-convention/templates/registry/semantic_attributes-h.j2
 */

#pragma once

#include "opentelemetry/common/macros.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace semconv
{
namespace db
{

/**
  The name of a collection (table, container) within the database.
  <p>
  It is RECOMMENDED to capture the value as provided by the application
  without attempting to do any case normalization.
  <p>
  The collection name SHOULD NOT be extracted from @code db.query.text @endcode,
  when the database system supports query text with multiple collections
  in non-batch operations.
  <p>
  For batch operations, if the individual operations are known to have the same
  collection name then that collection name SHOULD be used.
 */
static constexpr const char *kDbCollectionName = "db.collection.name";

/**
  The name of the database, fully qualified within the server address and port.
  <p>
  If a database system has multiple namespace components, they SHOULD be concatenated from the most
  general to the most specific namespace component, using @code | @endcode as a separator between
  the components. Any missing components (and their associated separators) SHOULD be omitted.
  Semantic conventions for individual database systems SHOULD document what @code db.namespace
  @endcode means in the context of that system. It is RECOMMENDED to capture the value as provided
  by the application without attempting to do any case normalization.
 */
static constexpr const char *kDbNamespace = "db.namespace";

/**
  The number of queries included in a batch operation.
  <p>
  Operations are only considered batches when they contain two or more operations, and so @code
  db.operation.batch.size @endcode SHOULD never be @code 1 @endcode.
 */
static constexpr const char *kDbOperationBatchSize = "db.operation.batch.size";

/**
  The name of the operation or command being executed.
  <p>
  It is RECOMMENDED to capture the value as provided by the application
  without attempting to do any case normalization.
  <p>
  The operation name SHOULD NOT be extracted from @code db.query.text @endcode,
  when the database system supports query text with multiple operations
  in non-batch operations.
  <p>
  If spaces can occur in the operation name, multiple consecutive spaces
  SHOULD be normalized to a single space.
  <p>
  For batch operations, if the individual operations are known to have the same operation name
  then that operation name SHOULD be used prepended by @code BATCH  @endcode,
  otherwise @code db.operation.name @endcode SHOULD be @code BATCH @endcode or some other database
  system specific term if more applicable.
 */
static constexpr const char *kDbOperationName = "db.operation.name";

/**
  Low cardinality summary of a database query.
  <p>
  The query summary describes a class of database queries and is useful
  as a grouping key, especially when analyzing telemetry for database
  calls involving complex queries.
  <p>
  Summary may be available to the instrumentation through
  instrumentation hooks or other means. If it is not available, instrumentations
  that support query parsing SHOULD generate a summary following
  <a href="/docs/database/database-spans.md#generating-a-summary-of-the-query">Generating query
  summary</a> section.
 */
static constexpr const char *kDbQuerySummary = "db.query.summary";

/**
  The database query being executed.
  <p>
  For sanitization see <a
  href="/docs/database/database-spans.md#sanitization-of-dbquerytext">Sanitization of @code
  db.query.text @endcode</a>. For batch operations, if the individual operations are known to have
  the same query text then that query text SHOULD be used, otherwise all of the individual query
  texts SHOULD be concatenated with separator @code ;  @endcode or some other database system
  specific separator if more applicable. Parameterized query text SHOULD NOT be sanitized. Even
  though parameterized query text can potentially have sensitive data, by using a parameterized
  query the user is giving a strong signal that any sensitive data will be passed as parameter
  values, and the benefit to observability of capturing the static part of the query text by default
  outweighs the risk.
 */
static constexpr const char *kDbQueryText = "db.query.text";

/**
  Database response status code.
  <p>
  The status code returned by the database. Usually it represents an error code, but may also
  represent partial success, warning, or differentiate between various types of successful outcomes.
  Semantic conventions for individual database systems SHOULD document what @code
  db.response.status_code @endcode means in the context of that system.
 */
static constexpr const char *kDbResponseStatusCode = "db.response.status_code";

/**
  The name of a stored procedure within the database.
  <p>
  It is RECOMMENDED to capture the value as provided by the application
  without attempting to do any case normalization.
  <p>
  For batch operations, if the individual operations are known to have the same
  stored procedure name then that stored procedure name SHOULD be used.
 */
static constexpr const char *kDbStoredProcedureName = "db.stored_procedure.name";

/**
  The database management system (DBMS) product as identified by the client instrumentation.
  <p>
  The actual DBMS may differ from the one identified by the client. For example, when using
  PostgreSQL client libraries to connect to a CockroachDB, the @code db.system.name @endcode is set
  to @code postgresql @endcode based on the instrumentation's best knowledge.
 */
static constexpr const char *kDbSystemName = "db.system.name";

namespace DbSystemNameValues
{
/**
  Some other SQL database. Fallback only.
 */
static constexpr const char *kOtherSql = "other_sql";

/**
  <a href="https://documentation.softwareag.com/?pf=adabas">Adabas (Adaptable Database System)</a>
 */
static constexpr const char *kSoftwareagAdabas = "softwareag.adabas";

/**
  <a href="https://www.actian.com/databases/ingres/">Actian Ingres</a>
 */
static constexpr const char *kActianIngres = "actian.ingres";

/**
  <a href="https://aws.amazon.com/pm/dynamodb/">Amazon DynamoDB</a>
 */
static constexpr const char *kAwsDynamodb = "aws.dynamodb";

/**
  <a href="https://aws.amazon.com/redshift/">Amazon Redshift</a>
 */
static constexpr const char *kAwsRedshift = "aws.redshift";

/**
  <a href="https://learn.microsoft.com/azure/cosmos-db">Azure Cosmos DB</a>
 */
static constexpr const char *kAzureCosmosdb = "azure.cosmosdb";

/**
  <a href="https://www.intersystems.com/products/cache/">InterSystems Caché</a>
 */
static constexpr const char *kIntersystemsCache = "intersystems.cache";

/**
  <a href="https://cassandra.apache.org/">Apache Cassandra</a>
 */
static constexpr const char *kCassandra = "cassandra";

/**
  <a href="https://clickhouse.com/">ClickHouse</a>
 */
static constexpr const char *kClickhouse = "clickhouse";

/**
  <a href="https://www.cockroachlabs.com/">CockroachDB</a>
 */
static constexpr const char *kCockroachdb = "cockroachdb";

/**
  <a href="https://www.couchbase.com/">Couchbase</a>
 */
static constexpr const char *kCouchbase = "couchbase";

/**
  <a href="https://couchdb.apache.org/">Apache CouchDB</a>
 */
static constexpr const char *kCouchdb = "couchdb";

/**
  <a href="https://db.apache.org/derby/">Apache Derby</a>
 */
static constexpr const char *kDerby = "derby";

/**
  <a href="https://www.elastic.co/elasticsearch">Elasticsearch</a>
 */
static constexpr const char *kElasticsearch = "elasticsearch";

/**
  <a href="https://www.firebirdsql.org/">Firebird</a>
 */
static constexpr const char *kFirebirdsql = "firebirdsql";

/**
  <a href="https://cloud.google.com/spanner">Google Cloud Spanner</a>
 */
static constexpr const char *kGcpSpanner = "gcp.spanner";

/**
  <a href="https://geode.apache.org/">Apache Geode</a>
 */
static constexpr const char *kGeode = "geode";

/**
  <a href="https://h2database.com/">H2 Database</a>
 */
static constexpr const char *kH2database = "h2database";

/**
  <a href="https://hbase.apache.org/">Apache HBase</a>
 */
static constexpr const char *kHbase = "hbase";

/**
  <a href="https://hive.apache.org/">Apache Hive</a>
 */
static constexpr const char *kHive = "hive";

/**
  <a href="https://hsqldb.org/">HyperSQL Database</a>
 */
static constexpr const char *kHsqldb = "hsqldb";

/**
  <a href="https://www.ibm.com/db2">IBM Db2</a>
 */
static constexpr const char *kIbmDb2 = "ibm.db2";

/**
  <a href="https://www.ibm.com/products/informix">IBM Informix</a>
 */
static constexpr const char *kIbmInformix = "ibm.informix";

/**
  <a href="https://www.ibm.com/products/netezza">IBM Netezza</a>
 */
static constexpr const char *kIbmNetezza = "ibm.netezza";

/**
  <a href="https://www.influxdata.com/">InfluxDB</a>
 */
static constexpr const char *kInfluxdb = "influxdb";

/**
  <a href="https://www.instantdb.com/">Instant</a>
 */
static constexpr const char *kInstantdb = "instantdb";

/**
  <a href="https://mariadb.org/">MariaDB</a>
 */
static constexpr const char *kMariadb = "mariadb";

/**
  <a href="https://memcached.org/">Memcached</a>
 */
static constexpr const char *kMemcached = "memcached";

/**
  <a href="https://www.mongodb.com/">MongoDB</a>
 */
static constexpr const char *kMongodb = "mongodb";

/**
  <a href="https://www.microsoft.com/sql-server">Microsoft SQL Server</a>
 */
static constexpr const char *kMicrosoftSqlServer = "microsoft.sql_server";

/**
  <a href="https://www.mysql.com/">MySQL</a>
 */
static constexpr const char *kMysql = "mysql";

/**
  <a href="https://neo4j.com/">Neo4j</a>
 */
static constexpr const char *kNeo4j = "neo4j";

/**
  <a href="https://opensearch.org/">OpenSearch</a>
 */
static constexpr const char *kOpensearch = "opensearch";

/**
  <a href="https://www.oracle.com/database/">Oracle Database</a>
 */
static constexpr const char *kOracleDb = "oracle.db";

/**
  <a href="https://www.postgresql.org/">PostgreSQL</a>
 */
static constexpr const char *kPostgresql = "postgresql";

/**
  <a href="https://redis.io/">Redis</a>
 */
static constexpr const char *kRedis = "redis";

/**
  <a href="https://www.sap.com/products/technology-platform/hana/what-is-sap-hana.html">SAP HANA</a>
 */
static constexpr const char *kSapHana = "sap.hana";

/**
  <a href="https://maxdb.sap.com/">SAP MaxDB</a>
 */
static constexpr const char *kSapMaxdb = "sap.maxdb";

/**
  <a href="https://www.sqlite.org/">SQLite</a>
 */
static constexpr const char *kSqlite = "sqlite";

/**
  <a href="https://www.teradata.com/">Teradata</a>
 */
static constexpr const char *kTeradata = "teradata";

/**
  <a href="https://trino.io/">Trino</a>
 */
static constexpr const char *kTrino = "trino";

}  // namespace DbSystemNameValues

}  // namespace db
}  // namespace semconv
OPENTELEMETRY_END_NAMESPACE
