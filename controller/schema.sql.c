#define ZT_NETCONF_SCHEMA_SQL \
"CREATE TABLE Config (\n"\
"  k varchar(16) PRIMARY KEY NOT NULL,\n"\
"  v varchar(1024) NOT NULL\n"\
");\n"\
"\n"\
"CREATE TABLE IpAssignment (\n"\
"  networkId char(16) NOT NULL,\n"\
"  nodeId char(10) NOT NULL,\n"\
"  ip blob(16) NOT NULL,\n"\
"  ipNetmaskBits integer NOT NULL DEFAULT(0),\n"\
"  ipVersion integer NOT NULL DEFAULT(4)\n"\
");\n"\
"\n"\
"CREATE INDEX IpAssignment_networkId_ip ON IpAssignment (networkId, ip);\n"\
"\n"\
"CREATE INDEX IpAssignment_networkId_nodeId ON IpAssignment (networkId, nodeId);\n"\
"\n"\
"CREATE INDEX IpAssignment_networkId ON IpAssignment (networkId);\n"\
"\n"\
"CREATE TABLE IpAssignmentPool (\n"\
"  networkId char(16) NOT NULL,\n"\
"  ipNetwork blob(16) NOT NULL,\n"\
"  ipNetmaskBits integer NOT NULL,\n"\
"  ipVersion integer NOT NULL DEFAULT(4),\n"\
"  active integer NOT NULL DEFAULT(1)\n"\
");\n"\
"\n"\
"CREATE INDEX IpAssignmentPool_networkId ON IpAssignmentPool (networkId);\n"\
"\n"\
"CREATE TABLE Member (\n"\
"  networkId char(16) NOT NULL,\n"\
"  nodeId char(10) NOT NULL,\n"\
"  cachedNetconf blob(4096),\n"\
"  cachedNetconfRevision integer NOT NULL DEFAULT(0),\n"\
"  cachedNetconfTimestamp integer NOT NULL DEFAULT(0),\n"\
"  clientReportedRevision integer NOT NULL DEFAULT(0),\n"\
"  authorized integer NOT NULL DEFAULT(0),\n"\
"  activeBridge integer NOT NULL DEFAULT(0)\n"\
");\n"\
"\n"\
"CREATE INDEX Member_networkId ON Member (networkId);\n"\
"\n"\
"CREATE UNIQUE INDEX Member_networkId_nodeId ON Member (networkId, nodeId);\n"\
"\n"\
"CREATE TABLE MulticastRate (\n"\
"  networkId char(16) NOT NULL,\n"\
"  mgMac char(12) NOT NULL,\n"\
"  mgAdi integer NOT NULL DEFAULT(0),\n"\
"  preload integer NOT NULL,\n"\
"  maxBalance integer NOT NULL,\n"\
"  accrual integer NOT NULL\n"\
");\n"\
"\n"\
"CREATE INDEX MulticastRate_networkId ON MulticastRate (networkId);\n"\
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
"  revision integer NOT NULL DEFAULT(1)\n"\
");\n"\
"\n"\
"CREATE TABLE Relay (\n"\
"  networkId char(16) NOT NULL,\n"\
"  nodeId char(10) NOT NULL\n"\
");\n"\
"\n"\
"CREATE UNIQUE INDEX Relay_networkId_nodeId ON Relay (networkId, nodeId);\n"\
"\n"\
"CREATE TABLE Node (\n"\
"  id char(10) PRIMARY KEY NOT NULL,\n"\
"  identity varchar(4096) NOT NULL,\n"\
"  lastAt varchar(64),\n"\
"  lastSeen integer NOT NULL DEFAULT(0),\n"\
"  firstSeen integer NOT NULL DEFAULT(0)\n"\
");\n"\
"\n"\
"CREATE TABLE Rule (\n"\
"  networkId char(16) NOT NULL,\n"\
"  nodeId char(10),\n"\
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
"  \"action\" varchar(4096) NOT NULL DEFAULT('accept')\n"\
");\n"\
"\n"\
"CREATE INDEX Rule_networkId ON Rule (networkId);\n"\
""
