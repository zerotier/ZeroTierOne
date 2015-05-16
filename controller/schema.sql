CREATE TABLE Config (
  k varchar(16) PRIMARY KEY NOT NULL,
  v varchar(1024) NOT NULL
);

CREATE TABLE IpAssignment (
  networkId char(16) NOT NULL,
  nodeId char(10) NOT NULL,
  ip blob(16) NOT NULL,
  ipNetmaskBits integer NOT NULL DEFAULT(0),
  ipVersion integer NOT NULL DEFAULT(4)
);

CREATE INDEX IpAssignment_networkId_ip ON IpAssignment (networkId, ip);

CREATE INDEX IpAssignment_networkId_nodeId ON IpAssignment (networkId, nodeId);

CREATE INDEX IpAssignment_networkId ON IpAssignment (networkId);

CREATE TABLE IpAssignmentPool (
  networkId char(16) NOT NULL,
  ipNetwork blob(16) NOT NULL,
  ipNetmaskBits integer NOT NULL,
  ipVersion integer NOT NULL DEFAULT(4)
);

CREATE INDEX IpAssignmentPool_networkId ON IpAssignmentPool (networkId);

CREATE TABLE Member (
  networkId char(16) NOT NULL,
  nodeId char(10) NOT NULL,
  authorized integer NOT NULL DEFAULT(0),
  activeBridge integer NOT NULL DEFAULT(0)
);

CREATE INDEX Member_networkId ON Member (networkId);

CREATE INDEX Member_networkId_activeBridge ON Member(networkId, activeBridge);

CREATE UNIQUE INDEX Member_networkId_nodeId ON Member (networkId, nodeId);

CREATE TABLE MulticastRate (
  networkId char(16) NOT NULL,
  mgMac char(12) NOT NULL,
  mgAdi integer NOT NULL DEFAULT(0),
  preload integer NOT NULL,
  maxBalance integer NOT NULL,
  accrual integer NOT NULL
);

CREATE INDEX MulticastRate_networkId ON MulticastRate (networkId);

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
  revision integer NOT NULL DEFAULT(1)
);

CREATE TABLE Relay (
  networkId char(16) NOT NULL,
  nodeId char(10) NOT NULL,
  phyAddress varchar(64) NOT NULL
);

CREATE INDEX Relay_networkId ON Relay (networkId);

CREATE UNIQUE INDEX Relay_networkId_nodeId ON Relay (networkId, nodeId);

CREATE TABLE Node (
  id char(10) PRIMARY KEY NOT NULL,
  identity varchar(4096) NOT NULL,
  lastAt varchar(64),
  lastSeen integer NOT NULL DEFAULT(0),
  firstSeen integer NOT NULL DEFAULT(0)
);

CREATE TABLE Rule (
  networkId char(16) NOT NULL,
  ruleId integer NOT NULL,
  nodeId char(10),
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
  "action" varchar(4096) NOT NULL DEFAULT('accept')
);

CREATE INDEX Rule_networkId ON Rule (networkId);