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
  revision integer NOT NULL DEFAULT(1)
);

CREATE TABLE Node (
  id char(10) PRIMARY KEY NOT NULL,
  identity varchar(4096) NOT NULL,
  lastAt varchar(64),
  lastSeen integer NOT NULL DEFAULT(0),
  firstSeen integer NOT NULL DEFAULT(0)
);

CREATE TABLE IpAssignment (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,
  ip blob(16) NOT NULL,
  ipNetmaskBits integer NOT NULL DEFAULT(0),
  ipVersion integer NOT NULL DEFAULT(4)
);

CREATE UNIQUE INDEX IpAssignment_networkId_ip ON IpAssignment (networkId, ip);

CREATE INDEX IpAssignment_networkId_nodeId ON IpAssignment (networkId, nodeId);

CREATE TABLE IpAssignmentPool (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  ipNetwork blob(16) NOT NULL,
  ipNetmaskBits integer NOT NULL,
  ipVersion integer NOT NULL DEFAULT(4)
);

CREATE INDEX IpAssignmentPool_networkId ON IpAssignmentPool (networkId);

CREATE TABLE Member (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,
  authorized integer NOT NULL DEFAULT(0),
  activeBridge integer NOT NULL DEFAULT(0),
  PRIMARY KEY (networkId, nodeId)
);

CREATE INDEX Member_networkId_activeBridge ON Member(networkId, activeBridge);

CREATE TABLE MulticastRate (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  mgMac char(12) NOT NULL,
  mgAdi integer NOT NULL DEFAULT(0),
  preload integer NOT NULL,
  maxBalance integer NOT NULL,
  accrual integer NOT NULL
);

CREATE INDEX MulticastRate_networkId ON MulticastRate (networkId);

CREATE TABLE Relay (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,
  phyAddress varchar(64) NOT NULL,
  PRIMARY KEY (networkId, nodeId)
);

CREATE INDEX Relay_networkId ON Relay (networkId);

CREATE TABLE Rule (
  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,
  ruleNo integer NOT NULL,
  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,
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
