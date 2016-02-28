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
  v4AssignMode varchar(8) NOT NULL DEFAULT('none'),
  v6AssignMode varchar(8) NOT NULL DEFAULT('none'),
  multicastLimit integer NOT NULL DEFAULT(32),
  creationTime integer NOT NULL DEFAULT(0),
  revision integer NOT NULL DEFAULT(1),
  memberRevisionCounter integer NOT NULL DEFAULT(1)
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

CREATE TABLE NodeHistory (
  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  networkVisitCounter INTEGER NOT NULL DEFAULT(0),
  networkRequestAuthorized INTEGER NOT NULL DEFAULT(0),
  requestTime INTEGER NOT NULL DEFAULT(0),
  clientMajorVersion INTEGER NOT NULL DEFAULT(0),
  clientMinorVersion INTEGER NOT NULL DEFAULT(0),
  clientRevision INTEGER NOT NULL DEFAULT(0),
  networkRequestMetaData VARCHAR(1024),
  fromAddress VARCHAR(128)
);

CREATE INDEX NodeHistory_nodeId ON NodeHistory (nodeId);
CREATE INDEX NodeHistory_networkId ON NodeHistory (networkId);
CREATE INDEX NodeHistory_requestTime ON NodeHistory (requestTime);

CREATE TABLE Gateway (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  ip blob(16) NOT NULL,
  ipVersion integer NOT NULL DEFAULT(4),
  metric integer NOT NULL DEFAULT(0)
);

CREATE UNIQUE INDEX Gateway_networkId_ip ON Gateway (networkId, ip);

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
  PRIMARY KEY (networkId, nodeId)
);

CREATE INDEX Member_networkId_activeBridge ON Member(networkId, activeBridge);
CREATE INDEX Member_networkId_memberRevision ON Member(networkId, memberRevision);

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
