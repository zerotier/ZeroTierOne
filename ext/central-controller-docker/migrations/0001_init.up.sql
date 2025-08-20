-- inits controller db schema

CREATE TABLE IF NOT EXISTS controllers_ctl (
    id text NOT NULL PRIMARY KEY,
    hostname text,
    last_heartbeat timestamp with time zone,
    public_identity text NOT NULL,
    version text
);

CREATE TABLE IF NOT EXISTS networks_ctl (
    id character varying(22) NOT NULL PRIMARY KEY,
    name text NOT NULL,
    configuration jsonb DEFAULT '{}'::jsonb NOT NULL,
    controller_id text REFERENCES controllers_ctl(id),
    revision integer DEFAULT 0 NOT NULL,
    last_modified timestamp with time zone DEFAULT now(),
    creation_time timestamp with time zone DEFAULT now()
);

CREATE TABLE IF NOT EXISTS network_memberships_ctl (
    device_id character varying(22) NOT NULL,
    network_id character varying(22) NOT NULL REFERENCES networks_ctl(id),
    authorized boolean,
    active_bridge boolean,
    ip_assignments text[],
    no_auto_assign_ips boolean,
    sso_exempt boolean,
    authentication_expiry_time timestamp with time zone,
    capabilities jsonb,
    creation_time timestamp with time zone DEFAULT now(),
    last_modified timestamp with time zone DEFAULT now(),
    identity text DEFAULT ''::text,
    last_authorized_credential text,
    last_authorized_time timestamp with time zone,
    last_deauthorized_time timestamp with time zone,
    last_seen jsonb DEFAULT '{}'::jsonb NOT NULL, -- in the context of the network
    remote_trace_level integer DEFAULT 0 NOT NULL,
    remote_trace_target text DEFAULT ''::text NOT NULL,
    revision integer DEFAULT 0 NOT NULL,
    tags jsonb,
    version_major integer DEFAULT 0 NOT NULL,
    version_minor integer DEFAULT 0 NOT NULL,
    version_revision integer DEFAULT 0 NOT NULL,
    version_protocol integer DEFAULT 0 NOT NULL,
    PRIMARY KEY (device_id, network_id)
);
