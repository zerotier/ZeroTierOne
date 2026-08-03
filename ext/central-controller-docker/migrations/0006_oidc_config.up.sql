ALTER TABLE sso_expiry RENAME COLUMN member_id TO device_id;
-- NOTE (2026-08-03): sso_expiry_network_member_ix is dropped again in 0008. It looks
-- correct but could never serve the ON DELETE CASCADE from network_memberships_ctl --
-- sso_expiry's columns are bpchar against a varchar parent, so the RI trigger compares
-- network_id::text and no bare-column index can match it. Do not re-add a bare-column
-- index here expecting it to help; see 0008_sso_expiry_indexes.up.sql for the full story.
CREATE INDEX IF NOT EXISTS sso_expiry_network_member_ix ON public.sso_expiry (network_id, device_id);

CREATE TABLE IF NOT EXISTS oidc_config (
	client_id TEXT NOT NULL,
	linked_id TEXT NOT NULL,
	issuer TEXT NOT NULL,
	authorization_endpoint TEXT NOT NULL,
	sso_impl_version BIGINT NOT NULL DEFAULT 1,
	provider TEXT NOT NULL DEFAULT 'default',
	PRIMARY KEY (client_id, linked_id)
);