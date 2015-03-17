#define ZT_NETCONF_SCHEMA_SQL \
"CREATE TABLE Config (\n"\
"  k varchar(16) PRIMARY KEY NOT NULL,\n"\
"  v varchar(1024) NOT NULL\n"\
");\n"\
"\n"\
"CREATE TABLE IpAssignment (\n"\
"  networkId char(16) NOT NULL,\n"\
"  nodeId char(10) NOT NULL,\n"\
"  ip varchar(64) NOT NULL,\n"\
"  ipNetmaskBits integer(4) NOT NULL DEFAULT(0)\n"\
");\n"\
"\n"\
"CREATE UNIQUE INDEX IpAssignment_networkId_ip ON IpAssignment (networkId, ip);\n"\
"\n"\
"CREATE INDEX IpAssignment_networkId_nodeId ON IpAssignment (networkId, nodeId);\n"\
"\n"\
"CREATE INDEX IpAssignment_networkId ON IpAssignment (networkId);\n"\
"\n"\
"CREATE TABLE IpAssignmentPool (\n"\
"  networkId char(16) NOT NULL,\n"\
"  ipNetwork varchar(64) NOT NULL,\n"\
"  ipNetmaskBits integer(4) NOT NULL,\n"\
"  active integer(1) NOT NULL DEFAULT(1)\n"\
");\n"\
"\n"\
"CREATE INDEX IpAssignmentPool_networkId ON IpAssignmentPool (networkId);\n"\
"\n"\
"CREATE TABLE Member (\n"\
"  networkId char(16) NOT NULL,\n"\
"  nodeId char(10) NOT NULL,\n"\
"  cachedNetconf blob(4096),\n"\
"  cachedNetconfRevision integer(32),\n"\
"  clientReportedRevision integer(32),\n"\
"  authorized integer(1) NOT NULL DEFAULT(0),\n"\
"  activeBridge integer(1) NOT NULL DEFAULT(0)\n"\
");\n"\
"\n"\
"CREATE INDEX Member_networkId ON Member (networkId);\n"\
"\n"\
"CREATE UNIQUE INDEX Member_networkId_nodeId ON Member (networkId, nodeId);\n"\
"\n"\
"CREATE TABLE MulticastRate (\n"\
"  networkId char(16) NOT NULL,\n"\
"  mgMac char(12) NOT NULL,\n"\
"  mgAdi integer(8) NOT NULL DEFAULT(0),\n"\
"  preload integer(16) NOT NULL,\n"\
"  maxBalance integer(16) NOT NULL,\n"\
"  accrual integer(16) NOT NULL\n"\
");\n"\
"\n"\
"CREATE INDEX MulticastRate_networkId ON MulticastRate (networkId);\n"\
"\n"\
"CREATE TABLE Network (\n"\
"  id char(16) PRIMARY KEY NOT NULL,\n"\
"  name varchar(128) NOT NULL,\n"\
"  private integer(1) NOT NULL DEFAULT(1),\n"\
"  enableBroadcast integer(1) NOT NULL DEFAULT(1),\n"\
"  allowPassiveBridging integer(1) NOT NULL DEFAULT(0),\n"\
"  v4AssignMode varchar(8) NOT NULL DEFAULT('none'),\n"\
"  v6AssignMode varchar(8) NOT NULL DEFAULT('none'),\n"\
"  multicastLimit integer(8) NOT NULL DEFAULT(32),\n"\
"  creationTime integer(32) NOT NULL DEFAULT(0),\n"\
"  revision integer(32) NOT NULL DEFAULT(0)\n"\
");\n"\
"\n"\
"CREATE TABLE Node (\n"\
"  id char(10) PRIMARY KEY NOT NULL,\n"\
"  identity varchar(4096) NOT NULL,\n"\
"  lastAt varchar(64),\n"\
"  lastSeen integer(32) NOT NULL DEFAULT(0),\n"\
"  firstSeen integer(32) NOT NULL DEFAULT(0)\n"\
");\n"\
"\n"\
"CREATE TABLE Rule (\n"\
"  networkId char(16) NOT NULL,\n"\
"  nodeId char(10),\n"\
"  vlanId integer(4),\n"\
"  vlanPcp integer(4),\n"\
"  etherType integer(8),\n"\
"  macSource char(12),\n"\
"  macDest char(12),\n"\
"  ipSource varchar(64),\n"\
"  ipDest varchar(64),\n"\
"  ipTos integer(4),\n"\
"  ipProtocol integer(4),\n"\
"  ipSourcePort integer(8),\n"\
"  ipDestPort integer(8),\n"\
"  "action" varchar(4096) NOT NULL DEFAULT('accept')\n"\
");\n"\
"\n"\
"CREATE INDEX Rule_networkId ON Rule (networkId);\n"\
""
