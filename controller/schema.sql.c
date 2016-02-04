#define ZT_NETCONF_SCHEMA_SQL \
"CREATE TABLE Config (\n"\
"  k varchar(16) PRIMARY KEY NOT NULL,\n"\
"  v varchar(1024) NOT NULL\n"\
");\n"\
"\n"\
"CREATE TABLE Network (\n"\
"  id char(16) PRIMARY KEY NOT NULL,\n"\
"  name varchar(128) NOT NULL,\n"\
"  private integer NOT NULL DEFAULT(1),\n"\
"  enableBroadcast integer NOT NULL DEFAULT(1),\n"\
"  allowPassiveBridging integer NOT NULL DEFAULT(0),\n"\
"  v4AssignMode varchar(8) NOT NULL DEFAULT('none'),\n"\
"  v6AssignMode varchar(8) NOT NULL DEFAULT('none'),\n"\
"  multicastLimit integer NOT NULL DEFAULT(32),\n"\
"  creationTime integer NOT NULL DEFAULT(0),\n"\
"  revision integer NOT NULL DEFAULT(1),\n"\
"  memberRevisionCounter integer NOT NULL DEFAULT(1)\n"\
");\n"\
"\n"\
"CREATE TABLE AuthToken (\n"\
"  id integer PRIMARY KEY NOT NULL,\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  authMode integer NOT NULL DEFAULT(1),\n"\
"  useCount integer NOT NULL DEFAULT(0),\n"\
"  maxUses integer NOT NULL DEFAULT(0),\n"\
"  expiresAt integer NOT NULL DEFAULT(0),\n"\
"  token varchar(256) NOT NULL\n"\
");\n"\
"\n"\
"CREATE INDEX AuthToken_networkId_token ON AuthToken(networkId,token);\n"\
"\n"\
"CREATE TABLE Node (\n"\
"  id char(10) PRIMARY KEY NOT NULL,\n"\
"  identity varchar(4096) NOT NULL\n"\
");\n"\
"\n"\
"CREATE TABLE NodeHistory (\n"\
"  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  networkVisitCounter INTEGER NOT NULL DEFAULT(0),\n"\
"  networkRequestAuthorized INTEGER NOT NULL DEFAULT(0),\n"\
"  requestTime INTEGER NOT NULL DEFAULT(0),\n"\
"  clientMajorVersion INTEGER NOT NULL DEFAULT(0),\n"\
"  clientMinorVersion INTEGER NOT NULL DEFAULT(0),\n"\
"  clientRevision INTEGER NOT NULL DEFAULT(0),\n"\
"  networkRequestMetaData VARCHAR(1024),\n"\
"  fromAddress VARCHAR(128)\n"\
");\n"\
"\n"\
"CREATE INDEX NodeHistory_nodeId ON NodeHistory (nodeId);\n"\
"CREATE INDEX NodeHistory_networkId ON NodeHistory (networkId);\n"\
"CREATE INDEX NodeHistory_requestTime ON NodeHistory (requestTime);\n"\
"\n"\
"CREATE TABLE Gateway (\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  ip blob(16) NOT NULL,\n"\
"  ipVersion integer NOT NULL DEFAULT(4),\n"\
"  metric integer NOT NULL DEFAULT(0)\n"\
");\n"\
"\n"\
"CREATE UNIQUE INDEX Gateway_networkId_ip ON Gateway (networkId, ip);\n"\
"\n"\
"CREATE TABLE IpAssignment (\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  nodeId char(10) REFERENCES Node(id) ON DELETE CASCADE,\n"\
"  type integer NOT NULL DEFAULT(0),\n"\
"  ip blob(16) NOT NULL,\n"\
"  ipNetmaskBits integer NOT NULL DEFAULT(0),\n"\
"  ipVersion integer NOT NULL DEFAULT(4)\n"\
");\n"\
"\n"\
"CREATE UNIQUE INDEX IpAssignment_networkId_ip ON IpAssignment (networkId, ip);\n"\
"\n"\
"CREATE INDEX IpAssignment_networkId_nodeId ON IpAssignment (networkId, nodeId);\n"\
"\n"\
"CREATE TABLE IpAssignmentPool (\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  ipRangeStart blob(16) NOT NULL,\n"\
"  ipRangeEnd blob(16) NOT NULL,\n"\
"  ipVersion integer NOT NULL DEFAULT(4)\n"\
");\n"\
"\n"\
"CREATE UNIQUE INDEX IpAssignmentPool_networkId_ipRangeStart ON IpAssignmentPool (networkId,ipRangeStart);\n"\
"\n"\
"CREATE TABLE Member (\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,\n"\
"  authorized integer NOT NULL DEFAULT(0),\n"\
"  activeBridge integer NOT NULL DEFAULT(0),\n"\
"  memberRevision integer NOT NULL DEFAULT(0),\n"\
"  PRIMARY KEY (networkId, nodeId)\n"\
");\n"\
"\n"\
"CREATE INDEX Member_networkId_activeBridge ON Member(networkId, activeBridge);\n"\
"CREATE INDEX Member_networkId_memberRevision ON Member(networkId, memberRevision);\n"\
"\n"\
"CREATE TABLE Relay (\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  address char(10) NOT NULL,\n"\
"  phyAddress varchar(64) NOT NULL\n"\
");\n"\
"\n"\
"CREATE UNIQUE INDEX Relay_networkId_address ON Relay (networkId,address);\n"\
"\n"\
"CREATE TABLE Rule (\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  ruleNo integer NOT NULL,\n"\
"  nodeId char(10) REFERENCES Node(id),\n"\
"  sourcePort char(10),\n"\
"  destPort char(10),\n"\
"  vlanId integer,\n"\
"  vlanPcp integer,\n"\
"  etherType integer,\n"\
"  macSource char(12),\n"\
"  macDest char(12),\n"\
"  ipSource varchar(64),\n"\
"  ipDest varchar(64),\n"\
"  ipTos integer,\n"\
"  ipProtocol integer,\n"\
"  ipSourcePort integer,\n"\
"  ipDestPort integer,\n"\
"  flags integer,\n"\
"  invFlags integer,\n"\
"  \"action\" varchar(4096) NOT NULL DEFAULT('accept')\n"\
");\n"\
"\n"\
"CREATE UNIQUE INDEX Rule_networkId_ruleNo ON Rule (networkId, ruleNo);\n"\
""
