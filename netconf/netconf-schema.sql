CREATE TABLE Config (
  k varchar(16) PRIMARY KEY NOT NULL,
  v varchar(1024) NOT NULL
) WITHOUT ROWID;

CREATE TABLE IpAssignment (
  networkId char(16) NOT NULL,
  nodeId char(10) NOT NULL,
  ip varchar(64) NOT NULL,
  ipNetmaskBits integer(4) NOT NULL DEFAULT(0)
);

CREATE INDEX IpAssignment_networkId ON IpAssignment (networkId);

CREATE INDEX IpAssignment_networkId_nodeId ON IpAssignment (networkId, nodeId);

CREATE UNIQUE INDEX IpAssignment_networkId_ip ON IpAssignment (networkId, ip);

CREATE TABLE IpAssignmentPool (
  networkId char(16) NOT NULL,
  ipNetwork varchar(64) NOT NULL,
  ipNetmaskBits integer(4) NOT NULL,
  active integer(1) NOT NULL DEFAULT(1)
);

CREATE INDEX IpAssignmentPool_networkId ON IpAssignmentPool (networkId);

CREATE TABLE Member (
  networkId char(16) NOT NULL,
  nodeId char(10) NOT NULL,
  cachedNetconf blob(4096),
  cachedNetconfRevision integer(32),
  cachedNetconfTimestamp integer(32),
  clientReportedTimestamp integer(32),
  authorized integer(1) NOT NULL DEFAULT(0),
  activeBridge integer(1) NOT NULL DEFAULT(0)
);

CREATE UNIQUE INDEX Member_networkId_nodeId ON Member (networkId, nodeId);

CREATE INDEX Member_networkId ON Member (networkId ASC);

CREATE TABLE Network (
  id char(16) PRIMARY KEY NOT NULL,
  name varchar(128) NOT NULL,
  private integer(1) NOT NULL DEFAULT(1),
  enableBroadcast integer(1) NOT NULL DEFAULT(1),
  allowPassiveBridging integer(1) NOT NULL DEFAULT(0),
  v4AssignMode varchar(8) NOT NULL DEFAULT('none'),
  v6AssignMode varchar(8) NOT NULL DEFAULT('none'),
  multicastLimit integer(8) NOT NULL DEFAULT(32),
  creationTime integer(32) NOT NULL DEFAULT(0),
  revision integer(32) NOT NULL DEFAULT(0)
) WITHOUT ROWID;

CREATE TABLE Node (
  id char(10) PRIMARY KEY NOT NULL,
  identity varchar(4096) NOT NULL,
  lastAt varchar(64),
  lastSeen integer(32) NOT NULL DEFAULT(0),
  firstSeen integer(32) NOT NULL DEFAULT(0)
) WITHOUT ROWID;

CREATE TABLE Rule (
  networkId char(16) NOT NULL,
  nodeId char(10),
  vlanId integer(4),
  vlanPcp integer(4),
  etherType integer(8),
  macSource char(12),
  macDest char(12),
  ipSource varchar(64),
  ipDest varchar(64),
  ipTos integer(4),
  ipProtocol integer(4),
  ipSourcePort integer(8),
  ipDestPort integer(8),
  "action" varchar(4096) NOT NULL DEFAULT('accept')
);

CREATE INDEX Rule_networkId ON Rule (networkId);