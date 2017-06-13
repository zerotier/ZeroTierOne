'use strict';

var sqlite3 = require('sqlite3').verbose();
var fs = require('fs');
var async = require('async');

function blobToIPv4(b)
{
	if (!b)
		return null;
	if (b.length !== 16)
		return null;
	return b.readUInt8(12).toString()+'.'+b.readUInt8(13).toString()+'.'+b.readUInt8(14).toString()+'.'+b.readUInt8(15).toString();
}
function blobToIPv6(b)
{
	if (!b)
		return null;
	if (b.length !== 16)
		return null;
	var s = '';
	for(var i=0;i<16;++i) {
		var x = b.readUInt8(i).toString(16);
		if (x.length === 1)
			s += '0';
		s += x;
		if ((((i+1) & 1) === 0)&&(i !== 15))
			s += ':';
	}
	return s;
}

if (process.argv.length !== 4) {
	console.log('ZeroTier Old Sqlite3 Controller DB Migration Utility');
	console.log('(c)2017 ZeroTier, Inc. [GPL3]');
	console.log('');
	console.log('Usage: node migrate.js </path/to/controller.db> </path/to/controller.d>');
	console.log('');
	console.log('The first argument must be the path to the old Sqlite3 controller.db');
	console.log('file. The second must be the path to the EMPTY controller.d database');
	console.log('directory for a new (1.1.17 or newer) controller. If this path does');
	console.log('not exist it will be created.');
	console.log('');
	console.log('WARNING: this will ONLY work correctly on a 1.1.14 controller database.');
	console.log('If your controller is old you should first upgrade to 1.1.14 and run the');
	console.log('controller so that it will brings its Sqlite3 database up to the latest');
	console.log('version before running this migration.');
	console.log('');
	process.exit(1);
}

var oldDbPath = process.argv[2];
var newDbPath = process.argv[3];

console.log('Starting migrate of "'+oldDbPath+'" to "'+newDbPath+'"...');
console.log('');

var old = new sqlite3.Database(oldDbPath);

var networks = {};

var nodeIdentities = {};
var networkCount = 0;
var memberCount = 0;
var routeCount = 0;
var ipAssignmentPoolCount = 0;
var ipAssignmentCount = 0;
var ruleCount = 0;
var oldSchemaVersion = -1;

async.series([function(nextStep) {

	old.each('SELECT v from Config WHERE k = \'schemaVersion\'',function(err,row) {
		oldSchemaVersion = parseInt(row.v)||-1;
	},nextStep);

},function(nextStep) {

	if (oldSchemaVersion !== 4) {
		console.log('FATAL: this MUST be run on a 1.1.14 controller.db! Upgrade your old');
		console.log('controller to 1.1.14 first and run it once to bring its DB up to date.');
		return process.exit(1);
	}

	console.log('Reading networks...');
	old.each('SELECT * FROM Network',function(err,row) {
		if ((typeof row.id === 'string')&&(row.id.length === 16)) {
			var flags = parseInt(row.flags)||0;
			networks[row.id] = {
				id: row.id,
				nwid: row.id,
				objtype: 'network',
				authTokens: [],
				capabilities: [],
				creationTime: parseInt(row.creationTime)||0,
				enableBroadcast: !!row.enableBroadcast,
				ipAssignmentPools: [],
				multicastLimit: row.multicastLimit||32,
				name: row.name||'',
				private: !!row.private,
				revision: parseInt(row.revision)||1,
				rules: [{ 'type': 'ACTION_ACCEPT' }], // populated later if there are defined rules, otherwise default is allow all
				routes: [],
				v4AssignMode: {
					'zt': ((flags & 1) !== 0)
				},
				v6AssignMode: {
					'6plane': ((flags & 4) !== 0),
					'rfc4193': ((flags & 2) !== 0),
					'zt': ((flags & 8) !== 0)
				},
				_members: {} // temporary
			};
			++networkCount;
			//console.log(networks[row.id]);
		}
	},nextStep);

},function(nextStep) {

	console.log('  '+networkCount+' networks.');
	console.log('Reading network route definitions...');
	old.each('SELECT * from Route WHERE ipVersion = 4 OR ipVersion = 6',function(err,row) {
		var network = networks[row.networkId];
		if (network) {
			var rt = {
				target: (((row.ipVersion == 4) ? blobToIPv4(row.target) : blobToIPv6(row.target))+'/'+row.targetNetmaskBits),
				via: ((row.via) ? ((row.ipVersion == 4) ? blobToIPv4(row.via) : blobToIPv6(row.via)) : null)
			};
			network.routes.push(rt);
			++routeCount;
		}
	},nextStep);

},function(nextStep) {

	console.log('  '+routeCount+' routes in '+networkCount+' networks.');
	console.log('Reading IP assignment pools...');
	old.each('SELECT * FROM IpAssignmentPool WHERE ipVersion = 4 OR ipVersion = 6',function(err,row) {
		var network = networks[row.networkId];
		if (network) {
			var p = {
				ipRangeStart: ((row.ipVersion == 4) ? blobToIPv4(row.ipRangeStart) : blobToIPv6(row.ipRangeStart)),
				ipRangeEnd: ((row.ipVersion == 4) ? blobToIPv4(row.ipRangeEnd) : blobToIPv6(row.ipRangeEnd))
			};
			network.ipAssignmentPools.push(p);
			++ipAssignmentPoolCount;
		}
	},nextStep);

},function(nextStep) {

	console.log('  '+ipAssignmentPoolCount+' IP assignment pools in '+networkCount+' networks.');
	console.log('Reading known node identities...');
	old.each('SELECT * FROM Node',function(err,row) {
		nodeIdentities[row.id] = row.identity;
	},nextStep);

},function(nextStep) {

	console.log('  '+Object.keys(nodeIdentities).length+' known identities.');
	console.log('Reading network members...');
	old.each('SELECT * FROM Member',function(err,row) {
		var network = networks[row.networkId];
		if (network) {
			network._members[row.nodeId] = {
				id: row.nodeId,
				address: row.nodeId,
				objtype: 'member',
				authorized: !!row.authorized,
				activeBridge: !!row.activeBridge,
				authHistory: [],
				capabilities: [],
				creationTime: 0,
				identity: nodeIdentities[row.nodeId]||null,
				ipAssignments: [],
				lastAuthorizedTime: (row.authorized) ? Date.now() : 0,
				lastDeauthorizedTime: (row.authorized) ? 0 : Date.now(),
				lastRequestMetaData: '',
				noAutoAssignIps: false,
				nwid: row.networkId,
				revision: parseInt(row.memberRevision)||1,
				tags: [],
				recentLog: []
			};
			++memberCount;
			//console.log(network._members[row.nodeId]);
		}
	},nextStep);

},function(nextStep) {

	console.log('  '+memberCount+' members of '+networkCount+' networks.');
	console.log('Reading static IP assignments...');
	old.each('SELECT * FROM IpAssignment WHERE ipVersion = 4 OR ipVersion = 6',function(err,row) {
		var network = networks[row.networkId];
		if (network) {
			var member = network._members[row.nodeId];
			if ((member)&&((member.authorized)||(!network['private']))) { // don't mirror assignments to unauthorized members to avoid conflicts
				if (row.ipVersion == 4) {
					member.ipAssignments.push(blobToIPv4(row.ip));
					++ipAssignmentCount;
				} else if (row.ipVersion == 6) {
					member.ipAssignments.push(blobToIPv6(row.ip));
					++ipAssignmentCount;
				}
			}
		}
	},nextStep);

},function(nextStep) {

	// Old versions only supported Ethertype whitelisting, so that's
	// all we mirror forward. The other fields were always unused.

	console.log('  '+ipAssignmentCount+' IP assignments for '+memberCount+' authorized members of '+networkCount+' networks.');
	console.log('Reading allowed Ethernet types (old basic rules)...');
	var etherTypesByNetwork = {};
	old.each('SELECT DISTINCT networkId,ruleNo,etherType FROM Rule WHERE "action" = \'accept\'',function(err,row) {
		if (row.networkId in networks) {
			var et = parseInt(row.etherType)||0;
			var ets = etherTypesByNetwork[row.networkId];
			if (!ets)
				etherTypesByNetwork[row.networkId] = [ et ];
			else ets.push(et);
		}
	},function(err) {
		if (err) return nextStep(err);
		for(var nwid in etherTypesByNetwork) {
			var ets = etherTypesByNetwork[nwid].sort();
			var network = networks[nwid];
			if (network) {
				var rules = [];
				if (ets.indexOf(0) >= 0) {
					// If 0 is in the list, all Ethernet types are allowed so we accept all.
					rules.push({ 'type': 'ACTION_ACCEPT' });
				} else {
					// Otherwise we whitelist.
					for(var i=0;i<ets.length;++i) {
						rules.push({
							'etherType': ets[i],
							'not': true,
							'or': false,
							'type': 'MATCH_ETHERTYPE'
						});
					}
					rules.push({ 'type': 'ACTION_DROP' });
					rules.push({ 'type': 'ACTION_ACCEPT' });
				}
				network.rules = rules;
				++ruleCount;
			}
		}
		return nextStep(null);
	});

}],function(err) {

	if (err) {
		console.log('FATAL: '+err.toString());
		return process.exit(1);
	}

	console.log('  '+ruleCount+' ethernet type whitelists converted to new format rules.');
	old.close();
	console.log('Done reading and converting Sqlite3 database! Writing JSONDB files...');

	try {
		fs.mkdirSync(newDbPath,0o700);
	} catch (e) {}
	var nwBase = newDbPath+'/network';
	try {
		fs.mkdirSync(nwBase,0o700);
	} catch (e) {}
	nwBase = nwBase + '/';
	var nwids = Object.keys(networks).sort();
	var fileCount = 0;
	for(var ni=0;ni<nwids.length;++ni) {
		var network = networks[nwids[ni]];

		var mids = Object.keys(network._members).sort();
		if (mids.length > 0) {
			try {
				fs.mkdirSync(nwBase+network.id);
			} catch (e) {}
			var mbase = nwBase+network.id+'/member';
			try {
				fs.mkdirSync(mbase,0o700);
			} catch (e) {}
			mbase = mbase + '/';

			for(var mi=0;mi<mids.length;++mi) {
				var member = network._members[mids[mi]];
				fs.writeFileSync(mbase+member.id+'.json',JSON.stringify(member,null,1),{ mode: 0o600 });
				++fileCount;
				//console.log(mbase+member.id+'.json');
			}
		}

		delete network._members; // temporary field, not part of actual JSONDB, so don't write
		fs.writeFileSync(nwBase+network.id+'.json',JSON.stringify(network,null,1),{ mode: 0o600 });
		++fileCount;
		//console.log(nwBase+network.id+'.json');
	}

	console.log('');
	console.log('SUCCESS! Wrote '+fileCount+' JSONDB files.');

	console.log('');
	console.log('You should still inspect the new DB before going live. Also be sure');
	console.log('to "chown -R" and "chgrp -R" the new DB to the user and group under');
	console.log('which the ZeroTier One instance acting as controller will be running.');
	console.log('The controller must be able to read and write the DB, of course.');
	console.log('');
	console.log('Have fun!');

	return process.exit(0);
});
