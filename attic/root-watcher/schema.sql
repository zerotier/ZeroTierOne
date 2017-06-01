/* Schema for ZeroTier root watcher log database */

CREATE TABLE "Peer"
(
	"ztAddress" BIGINT NOT NULL,
	"timestamp" BIGINT NOT NULL,
	"versionMajor" INTEGER NOT NULL,
	"versionMinor" INTEGER NOT NULL,
	"versionRev" INTEGER NOT NULL,
	"rootId" INTEGER NOT NULL,
	"phyPort" INTEGER NOT NULL,
	"phyLinkQuality" REAL NOT NULL,
	"phyLastReceive" BIGINT NOT NULL,
	"phyAddress" INET NOT NULL
);

CREATE INDEX "Peer_ztAddress" ON "Peer" ("ztAddress");
CREATE INDEX "Peer_timestamp" ON "Peer" ("timestamp");
CREATE INDEX "Peer_rootId" ON "Peer" ("rootId");
CREATE INDEX "Peer_phyAddress" ON "Peer" ("phyAddress");
