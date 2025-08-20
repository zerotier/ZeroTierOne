ALTER TABLE network_memberships_ctl
    ADD COLUMN os TEXT NOT NULL DEFAULT 'unknown',
    ADD COLUMN arch TEXT NOT NULL DEFAULT 'unknown';