-- sso_expiry index overhaul (2026-08-03).
--
-- WHY: sso_expiry.network_id / device_id are CHARACTER(16) / CHARACTER(10) (bpchar), while
-- the parent columns they reference -- network_memberships_ctl.network_id / device_id --
-- are character varying(22). There is no varchar = bpchar operator, so when the FK in 0005
-- was created PostgreSQL coerced both sides to text and stored texteq as the constraint's
-- comparison operator. The ON DELETE CASCADE trigger consequently emits:
--
--     DELETE FROM ONLY public.sso_expiry x
--     WHERE $1::pg_catalog.text OPERATOR(pg_catalog.=) x.network_id::pg_catalog.text
--       AND $2::pg_catalog.text OPERATOR(pg_catalog.=) x.device_id::pg_catalog.text
--
-- bpchar->text is a real function call (it strips trailing blanks), not a binary-coercible
-- relabel, so a btree index on the bare columns can NEVER match that qual. This is why
-- sso_expiry_network_member_ix (added in 0006, on exactly the right two columns) was never
-- once used by the cascade: every single member delete seq-scanned the entire table.
--
-- Measured in prod 2026-08-01: 64,974 buffers / 1.33 s per deleted member, at idle. A user
-- bulk-deleting ~2,100 members from one network pinned the 4-vCPU AlloyDB instance at 100%
-- CPU for 27 minutes and paged on controller_db_commit_latency_ms.
--
-- Note the asymmetry: the reverse RI direction (checking the parent exists on INSERT into
-- sso_expiry) stays fast at ~17 buffers, because varchar->text IS binary-coercible and the
-- planner strips it as a RelabelType. Same operator, same cast syntax, opposite outcomes.
--
-- WHAT:
--   sso_expiry_ri_ix     -- built on the cast expressions so the cascade can match it.
--                           Prod: 61,268 -> ~15 buffers, 1,450 ms -> 0.4 ms per delete.
--   sso_expiry_lookup_ix -- adds nonce_expiration as a third key column so the netconf-path
--                           nonce lookups (CentralDB.cpp, "SELECT nonce FROM sso_expiry
--                           WHERE network_id = $1 AND device_id = $2 AND ... <= ...") can
--                           range-seek past expired rows instead of heap-fetching every
--                           accumulated row for that member. A member that never completes
--                           SSO accrues one row per 5 minutes and rows are retained ~30
--                           days (sso_expiry doubles as the SSO log), so those groups run
--                           to thousands of rows.
--                           Prod: 4,850 -> 5 buffers, 16 ms -> 0.03 ms per lookup, which
--                           returned ~1.2 of the 4 vCPUs.
--   sso_expiry_creation_time_ix
--                        -- the retention pruner in central-v2/apps/controller-manager/
--                           core/controller/ssocleanup.go ("DELETE FROM sso_expiry WHERE
--                           creation_time < NOW() - INTERVAL '32 days'") had no index on
--                           creation_time and full-scanned the table on every run, roughly
--                           every 5 minutes, forever.
--                           Prod: 58,000 -> ~900 buffers, 880 ms -> 1.0 ms per run.
--   sso_expiry_network_member_ix is dropped: it is a strict prefix of sso_expiry_lookup_ix,
--                           so it serves nothing the new index does not, while costing
--                           maintenance on every insert and every pruner delete.
--
-- LOCKING: prod already has all three changes, applied by hand on 2026-08-03 using
-- CREATE INDEX CONCURRENTLY / DROP INDEX CONCURRENTLY, so nothing blocked. This file
-- deliberately uses plain, non-concurrent DDL: golang-migrate sends the whole file as a
-- single Exec, which PostgreSQL runs as an implicit transaction block, and
-- CREATE INDEX CONCURRENTLY cannot run inside a transaction block. That is the right
-- trade-off for the databases this file will actually run against -- dev, staging, and
-- freshly provisioned controllers -- where sso_expiry is small or empty.
--
-- Do NOT run this file against a large populated database. CREATE INDEX takes a SHARE lock
-- (blocking writes to sso_expiry for the duration of each build) and DROP INDEX takes
-- ACCESS EXCLUSIVE. At prod scale, do it by hand with CONCURRENTLY instead.

CREATE INDEX IF NOT EXISTS sso_expiry_ri_ix
    ON public.sso_expiry ((network_id::text), (device_id::text));

CREATE INDEX IF NOT EXISTS sso_expiry_lookup_ix
    ON public.sso_expiry (network_id, device_id, nonce_expiration);

CREATE INDEX IF NOT EXISTS sso_expiry_creation_time_ix
    ON public.sso_expiry (creation_time);

DROP INDEX IF EXISTS sso_expiry_network_member_ix;
