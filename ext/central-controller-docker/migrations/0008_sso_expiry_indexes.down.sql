-- Reverse of 0008: drop the two new indexes and restore sso_expiry_network_member_ix
-- exactly as 0006 created it.
--
-- WARNING: this reinstates the defect 0008 fixed. With sso_expiry_ri_ix gone, the
-- ON DELETE CASCADE from network_memberships_ctl falls back to a full sequential scan of
-- sso_expiry for every member deleted; with sso_expiry_lookup_ix gone the netconf-path
-- nonce lookups go back to heap-fetching every accumulated row per member; and with
-- sso_expiry_creation_time_ix gone the retention pruner full-scans the table every run.
-- See the header of 0008_sso_expiry_indexes.up.sql for the measurements.

DROP INDEX IF EXISTS sso_expiry_creation_time_ix;
DROP INDEX IF EXISTS sso_expiry_lookup_ix;
DROP INDEX IF EXISTS sso_expiry_ri_ix;

CREATE INDEX IF NOT EXISTS sso_expiry_network_member_ix ON public.sso_expiry (network_id, device_id);
