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
"  revision integer NOT NULL DEFAULT(1)\n"\
");\n"\
"\n"\
"CREATE TABLE Node (\n"\
"  id char(10) PRIMARY KEY NOT NULL,\n"\
"  identity varchar(4096) NOT NULL,\n"\
"  lastAt varchar(64),\n"\
"  lastSeen integer NOT NULL DEFAULT(0),\n"\
"  firstSeen integer NOT NULL DEFAULT(0)\n"\
");\n"\
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
"  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,\n"\
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
"  ipNetwork blob(16) NOT NULL,\n"\
"  ipNetmaskBits integer NOT NULL,\n"\
"  ipVersion integer NOT NULL DEFAULT(4)\n"\
");\n"\
"\n"\
"CREATE INDEX IpAssignmentPool_networkId ON IpAssignmentPool (networkId);\n"\
"\n"\
"CREATE TABLE Member (\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,\n"\
"  authorized integer NOT NULL DEFAULT(0),\n"\
"  activeBridge integer NOT NULL DEFAULT(0),\n"\
"  PRIMARY KEY (networkId, nodeId)\n"\
");\n"\
"\n"\
"CREATE INDEX Member_networkId_activeBridge ON Member(networkId, activeBridge);\n"\
"\n"\
"CREATE TABLE Relay (\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,\n"\
"  phyAddress varchar(64) NOT NULL,\n"\
"  PRIMARY KEY (networkId, nodeId)\n"\
");\n"\
"\n"\
"CREATE INDEX Relay_networkId ON Relay (networkId);\n"\
"\n"\
"CREATE TABLE Rule (\n"\
"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"\
"  ruleNo integer NOT NULL,\n"\
"  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,\n"\
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
