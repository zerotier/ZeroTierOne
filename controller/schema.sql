CREATE TABLE Config (
  k varchar(16) PRIMARY KEY NOT NULL,
  v varchar(1024) NOT NULL
);

CREATE TABLE Network (
  id char(16) PRIMARY KEY NOT NULL,
  name varchar(128) NOT NULL,
  private integer NOT NULL DEFAULT(1),
  enableBroadcast integer NOT NULL DEFAULT(1),
  allowPassiveBridging integer NOT NULL DEFAULT(0),
  multicastLimit integer NOT NULL DEFAULT(32),
  creationTime integer NOT NULL DEFAULT(0),
  revision integer NOT NULL DEFAULT(1),
  memberRevisionCounter integer NOT NULL DEFAULT(1),
  flags integer NOT NULL DEFAULT(0)
);

CREATE TABLE AuthToken (
  id integer PRIMARY KEY NOT NULL,
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  authMode integer NOT NULL DEFAULT(1),
  useCount integer NOT NULL DEFAULT(0),
  maxUses integer NOT NULL DEFAULT(0),
  expiresAt integer NOT NULL DEFAULT(0),
  token varchar(256) NOT NULL
);

CREATE INDEX AuthToken_networkId_token ON AuthToken(networkId,token);

CREATE TABLE Node (
  id char(10) PRIMARY KEY NOT NULL,
  identity varchar(4096) NOT NULL
);

CREATE TABLE IpAssignment (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  nodeId char(10) REFERENCES Node(id) ON DELETE CASCADE,
  type integer NOT NULL DEFAULT(0),
  ip blob(16) NOT NULL,
  ipNetmaskBits integer NOT NULL DEFAULT(0),
  ipVersion integer NOT NULL DEFAULT(4)
);

CREATE UNIQUE INDEX IpAssignment_networkId_ip ON IpAssignment (networkId, ip);

CREATE INDEX IpAssignment_networkId_nodeId ON IpAssignment (networkId, nodeId);

CREATE TABLE IpAssignmentPool (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  ipRangeStart blob(16) NOT NULL,
  ipRangeEnd blob(16) NOT NULL,
  ipVersion integer NOT NULL DEFAULT(4)
);

CREATE UNIQUE INDEX IpAssignmentPool_networkId_ipRangeStart ON IpAssignmentPool (networkId,ipRangeStart);

CREATE TABLE Member (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,
  authorized integer NOT NULL DEFAULT(0),
  activeBridge integer NOT NULL DEFAULT(0),
  memberRevision integer NOT NULL DEFAULT(0),
  flags integer NOT NULL DEFAULT(0),
  lastRequestTime integer NOT NULL DEFAULT(0),
  lastPowDifficulty integer NOT NULL DEFAULT(0),
  lastPowTime integer NOT NULL DEFAULT(0),
  recentHistory blob,
  PRIMARY KEY (networkId, nodeId)
);

CREATE INDEX Member_networkId_nodeId ON Member(networkId,nodeId);
CREATE INDEX Member_networkId_activeBridge ON Member(networkId, activeBridge);
CREATE INDEX Member_networkId_memberRevision ON Member(networkId, memberRevision);
CREATE INDEX Member_networkId_lastRequestTime ON Member(networkId, lastRequestTime);

CREATE TABLE Route (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  target blob(16) NOT NULL,
  via blob(16),
  targetNetmaskBits integer NOT NULL,
  ipVersion integer NOT NULL,
  flags integer NOT NULL,
  metric integer NOT NULL
);

CREATE INDEX Route_networkId ON Route (networkId);

CREATE TABLE Relay (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  address char(10) NOT NULL,
  phyAddress varchar(64) NOT NULL
);

CREATE UNIQUE INDEX Relay_networkId_address ON Relay (networkId,address);

CREATE TABLE Rule (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  ruleNo integer NOT NULL,
  nodeId char(10) REFERENCES Node(id),
  sourcePort char(10),
  destPort char(10),
  vlanId integer,
  vlanPcp integer,
  etherType integer,
  macSource char(12),
  macDest char(12),
  ipSource varchar(64),
  ipDest varchar(64),
  ipTos integer,
  ipProtocol integer,
  ipSourcePort integer,
  ipDestPort integer,
  flags integer,
  invFlags integer,
  "action" varchar(4096) NOT NULL DEFAULT('accept')
);

CREATE UNIQUE INDEX Rule_networkId_ruleNo ON Rule (networkId, ruleNo);
