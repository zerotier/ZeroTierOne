//
// ZeroTier One - Global Peer to Peer Ethernet
// Copyright (C) 2011-2014  ZeroTier Networks LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// --
//
// ZeroTier may be used and distributed under the terms of the GPLv3, which
// are available at: http://www.gnu.org/licenses/gpl-3.0.html
//
// If you would like to embed ZeroTier into a commercial application or
// redistribute it in a modified binary form, please contact ZeroTier Networks
// LLC. Start here: http://www.zerotier.com/
//

var config = require('./config.js');

// Fields in netconf response dictionary
var ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES = "et";
var ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID = "nwid";
var ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP = "ts";
var ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO = "id";
var ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT = "ml";
var ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES = "mr";
var ZT_NETWORKCONFIG_DICT_KEY_PRIVATE = "p";
var ZT_NETWORKCONFIG_DICT_KEY_NAME = "n";
var ZT_NETWORKCONFIG_DICT_KEY_DESC = "d";
var ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC = "v4s";
var ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC = "v6s";
var ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP = "com";
var ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST = "eb";
var ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING = "pb";
var ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES = "ab";

// Path to zerotier-idtool binary, invoked to enerate certificates of membership
var ZEROTIER_IDTOOL = '/usr/local/bin/zerotier-idtool';

// From Constants.hpp in node/
var ZT_NETWORK_AUTOCONF_DELAY = 60000;
var ZT_NETWORK_CERTIFICATE_TTL_WINDOW = (ZT_NETWORK_AUTOCONF_DELAY * 4);

// Connect to redis, assuming database 0 and no auth (for now)
var async = require('async');
var redis = require('redis');
var DB = redis.createClient();
DB.on("error",function(err) { console.error('redis query error: '+err); });
DB.select(config.redisDb,function() {});

// Global variables -- these are initialized on startup or netconf-init message
var netconfSigningIdentity = null; // identity of netconf master, with private key portion

// spawn() function to launch sub-processes
var spawn = require('child_process').spawn;

// Returns true for fields that are "true" according to ZT redis schema
function ztDbTrue(v) { return ((v === '1')||(v === 'true')||(v > 0)); }

//
// ZeroTier One Dictionary -- encoding-compatible with Dictionary in C++ code base
//

function Dictionary(fromStr)
{
	var self = this;

	this.data = {};

	this._esc = function(data) {
		var es = '';
		for(var i=0;i<data.length;++i) {
			var c = data.charAt(i);
			switch(c) {
				case '\0': es += '\\0'; break;
				case '\r': es += '\\r'; break;
				case '\n': es += '\\n'; break;
				case '\\': es += '\\\\'; break;
				case '=': es += '\\='; break;
				default: es += c; break;
			}
		}
		return es;
	};
	this._unesc = function(s) {
		if (typeof s !== 'string')
			return '';
		var uns = '';
		var escapeState = false;
		for(var i=0;i<s.length;++i) {
			var c = s.charAt(i);
			if (escapeState) {
				escapeState = false;
				switch(c) {
					case '0': uns += '\0'; break;
					case 'r': uns += '\r'; break;
					case 'n': uns += '\n'; break;
					default: uns += c; break;
				}
			} else{
				if ((c !== '\r')&&(c !== '\n')&&(c !== '\0')) {
					if (c === '\\')
						escapeState = true;
					else uns += c;
				}
			}
		}
		return uns;
	};

	this.toString = function() {
		var str = '';

		for(var key in self.data) {
			str += self._esc(key);
			str += '=';
			var value = self.data[key];
			if (value)
				str += self._esc(value.toString());
			str += '\n';
		}

		return str;
	};

	this.fromString = function(str) {
		self.data = {};
		if (typeof str !== 'string')
			return self;

		var lines = str.split('\n');
		for(var l=0;l<lines.length;++l) {
			var escapeState = false;
			var eqAt = 0;
			for(;eqAt<lines[l].length;++eqAt) {
				var c = lines[l].charAt(eqAt);
				if (escapeState)
					escapeState = false;
				else if (c === '\\')
					escapeState = true;
				else if (c === '=')
					break;
			}

			var k = self._unesc(lines[l].substr(0,eqAt));
			++eqAt;
			if ((k)&&(k.length > 0))
				self.data[k] = self._unesc((eqAt < lines[l].length) ? lines[l].substr(eqAt) : '');
		}

		return self;
	};

	if ((typeof fromStr === 'string')&&(fromStr.length > 0))
		self.fromString(fromStr);
};

//
// Identity implementation using zerotier-idtool as subprocess to do actual crypto work
//

function Identity(idstr)
{
	var self = this;

	this.str = '';
	this.fields = [];

	this.toString = function() {
		return self.str;
	};

	this.address = function() {
		return ((self.fields.length > 0) ? self.fields[0] : '0000000000');
	};

	this.fromString = function(str) {
		self.str = '';
		self.fields = [];
		if (typeof str !== 'string')
			return;
		for(var i=0;i<str.length;++i) {
			if ("0123456789abcdef:".indexOf(str.charAt(i)) < 0)
				return; // invalid character in identity
		}
		var fields = str.split(':');
		if ((fields.length < 3)||(fields[0].length !== 10)||(fields[1] !== '0'))
			return;
		self.str = str;
		self.fields = fields;
	};

	this.isValid = function() {
		return (! ((self.fields.length < 3)||(self.fields[0].length !== 10)||(self.fields[1] !== '0')) );
	};

	this.hasPrivate = function() {
		return ((self.isValid())&&(self.fields.length >= 4));
	};

	if (typeof idstr === 'string')
		self.fromString(idstr);
};

//
// Invokes zerotier-idtool to generate certificates for private networks
//

function generateCertificateOfMembership(nwid,peerAddress,callback)
{
	// The first fields of these COM tuples come from
	// CertificateOfMembership.hpp's enum of required
	// certificate default fields.
	var comTimestamp = '0,' + Date.now().toString(16) + ',' + ZT_NETWORK_CERTIFICATE_TTL_WINDOW.toString(16);
	var comNwid = '1,' + nwid + ',0';
	var comIssuedTo = '2,' + peerAddress + ',ffffffffffffffff';

	var cert = '';
	var certErr = '';

	var idtool = spawn(ZEROTIER_IDTOOL,[ 'mkcom',netconfSigningIdentity,comTimestamp,comNwid,comIssuedTo ]);
	idtool.stdout.on('data',function(data) {
		cert += data;
	});
	idtool.stderr.on('data',function(data) {
		certErr += data;
	});
	idtool.on('close',function(exitCode) {
		if (certErr.length > 0)
			console.error('zerotier-idtool stderr returned: '+certErr);
		return callback((cert.length > 0) ? cert : null,exitCode);
	});
}

//
// Message handler for messages over ZeroTier One service bus
//

function doNetconfInit(message)
{
	netconfSigningIdentity = new Identity(message.data['netconfId']);
	if (!netconfSigningIdentity.hasPrivate()) {
		netconfSigningIdentity = null;
		console.error('got invalid netconf signing identity in netconf-init');
	} // else console.error('got netconf-init, running! id: '+netconfSigningIdentity.address());
}

function doNetconfRequest(message)
{
	if ((netconfSigningIdentity === null)||(!netconfSigningIdentity.hasPrivate())) {
		console.error('got netconf-request before netconf-init, ignored');
		return;
	}

	var peerId = new Identity(message.data['peerId']);
	var nwid = message.data['nwid'];
	var requestId = message.data['requestId'];
	if ((!peerId)||(!peerId.isValid())||(!nwid)||(nwid.length !== 16)||(!requestId)) {
		console.error('missing one or more required fields in netconf-request');
		return;
	}

	var networkKey = 'zt1:network:'+nwid+':~';
	var memberKey = 'zt1:network:'+nwid+':member:'+peerId.address()+':~';
	var ipAssignmentsKey = 'zt1:network:'+nwid+':ipAssignments';

	var network = null;
	var member = null;

	var authorized = false;

	var v4NeedAssign = false;
	var v6NeedAssign = false;
	var v4Assignments = [];
	var v6Assignments = [];
	var ipAssignments = []; // both v4 and v6
	var activeBridges = '';

	async.series([function(next) {

		// network lookup
		DB.hgetall(networkKey,function(err,obj) {
			if ((!err)&&(obj)&&(obj.id === nwid))
				network = obj;
			return next(null);
		});

	},function(next) {

		// member lookup
		if (!network)
			return next(null);

		DB.hgetall(memberKey,function(err,obj) {
			if (err)
				return next(err);

			if (obj) {
				// Update existing member record with new last seen time, etc.
				member = obj;
				authorized = ((!ztDbTrue(network['private'])) || ztDbTrue(member['authorized']));
				var updatedFields = {
					'lastSeen': Date.now(),
					'authorized': authorized ? '1' : '0' // reset authorized to unhide in UI, since UI uses -1 to hide
				};
				if (!('identity' in member))
					updatedFields['identity'] = peerId.toString();
				if (!('firstSeen' in member))
					updatedFields['firstSeen'] = Date.now();
				if (message.data['from'])
					updatedFields['lastAt'] = message.data['from'];
				if (message.data['clientVersion'])
					updatedFields['clientVersion'] = message.data['clientVersion'];
				if (message.data['clientOs'])
					updatedFields['clientOs'] = message.data['clientOs'];
				DB.hmset(memberKey,updatedFields,next);
			} else {
				// Add member record to network for newly seen peer
				authorized = ztDbTrue(network['private']) ? false : true; // public networks authorize everyone by default
				var now = Date.now().toString();
				member = {
					'id': peerId.address(),
					'nwid': nwid,
					'authorized': authorized ? '1' : '0',
					'identity': peerId.toString(),
					'firstSeen': now,
					'lastSeen': now
				};
				if (message.data['from'])
					member['lastAt'] = message.data['from'];
				if (message.data['clientVersion'])
					member['clientVersion'] = message.data['clientVersion'];
				if (message.data['clientOs'])
					member['clientOs'] = message.data['clientOs'];
				DB.hmset(memberKey,member,next);
			}
		});

	},function(next) {

		// Figure out which IP address auto-assignments we need to look up or make
		if ((!network)||(!authorized))
			return next(null);

		v4NeedAssign = (network['v4AssignMode'] === 'zt');
		v6NeedAssign = (network['v6AssignMode'] === 'zt');

		var ipacsv = member['ipAssignments'];
		if (ipacsv) {
			var ipa = ipacsv.split(',');
			for(var i=0;i<ipa.length;++i) {
				if (ipa[i]) {
 					ipAssignments.push(ipa[i]);
					if ((ipa[i].indexOf('.') > 0)&&(v4NeedAssign))
						v4Assignments.push(ipa[i]);
					else if ((ipa[i].indexOf(':') > 0)&&(v6NeedAssign))
						v6Assignments.push(ipa[i]);
				}
			}
		}

		return next(null);

	},function(next) {

		// assign IPv4 if needed
		if ((!network)||(!authorized)||(!v4NeedAssign)||(v4Assignments.length > 0))
			return next(null);

		var peerAddress = peerId.address();

		var ipnetwork = 0;
		var netmask = 0;
		var netmaskBits = 0;
		var v4pool = network['v4AssignPool']; // technically csv but only one netblock currently supported
		if (v4pool) {
			var v4poolSplit = v4pool.split('/');
			if (v4poolSplit.length === 2) {
				var networkSplit = v4poolSplit[0].split('.');
				if (networkSplit.length === 4) {
					ipnetwork |= (parseInt(networkSplit[0],10) << 24) & 0xff000000;
					ipnetwork |= (parseInt(networkSplit[1],10) << 16) & 0x00ff0000;
					ipnetwork |= (parseInt(networkSplit[2],10) << 8) & 0x0000ff00;
					ipnetwork |= parseInt(networkSplit[3],10) & 0x000000ff;
					netmaskBits = parseInt(v4poolSplit[1],10);
					if (netmaskBits > 32)
						netmaskBits = 32; // sanity check
					for(var i=0;i<netmaskBits;++i)
						netmask |= (0x80000000 >> i);
					netmask &= 0xffffffff;
				}
			}
		}
		if ((ipnetwork === 0)||(netmask === 0xffffffff))
			return next(null);
		var invmask = netmask ^ 0xffffffff;

		var abcd = 0;
		var ipAssignmentAttempts = 0;

		async.whilst(
			function() { return ((v4Assignments.length === 0)&&(ipAssignmentAttempts < 1000)); },
			function(next2) {
				++ipAssignmentAttempts;

				// Generate or increment IP address source bits
				if (abcd === 0) {
					var a = parseInt(peerAddress.substr(2,2),16) & 0xff;
					var b = parseInt(peerAddress.substr(4,2),16) & 0xff;
					var c = parseInt(peerAddress.substr(6,2),16) & 0xff;
					var d = parseInt(peerAddress.substr(8,2),16) & 0xff;
					abcd = (a << 24) | (b << 16) | (c << 8) | d;
				} else ++abcd;
				if ((abcd & 0xff) === 0)
					abcd |= 1;
				abcd &= 0xffffffff;

				// Derive an IP to test and generate assignment ip/bits string
				var ip = (abcd & invmask) | (ipnetwork & netmask);
				var assignment = ((ip >> 24) & 0xff).toString(10) + '.' + ((ip >> 16) & 0xff).toString(10) + '.' + ((ip >> 8) & 0xff).toString(10) + '.' + (ip & 0xff).toString(10) + '/' + netmaskBits.toString(10);

				// Check :ipAssignments to see if this IP is already taken
				DB.hget(ipAssignmentsKey,assignment,function(err,value) {
					if (err)
						return next2(err);

					// IP is already taken, try again via async.whilst()
					if ((value)&&(value !== peerAddress))
						return next2(null); // if someone's already got this IP, keep looking

					v4Assignments.push(assignment);
					ipAssignments.push(assignment);

					// Save assignment to :ipAssignments hash
					DB.hset(ipAssignmentsKey,assignment,peerAddress,function(err) {
						if (err)
							return next2(err);

						// Save updated CSV list of assignments to member record
						var ipacsv = ipAssignments.join(',');
						member['ipAssignments'] = ipacsv;
						DB.hset(memberKey,'ipAssignments',ipacsv,next2);
					});
				});
			},
			next
		);

	},function(next) {

		// assign IPv6 if needed -- TODO
		if ((!network)||(!authorized)||(!v6NeedAssign)||(v6Assignments.length > 0))
			return next(null);

		return next(null);

	},function(next) {

		// Get active bridges
		if ((!network)||(!authorized))
			return next(null);

		DB.keys('zt1:network:'+nwid+':member:*:~',function(err,keys) {
			if (keys) {
				async.eachSeries(keys,function(key,nextKey) {
					DB.hgetall(key,function(err,abr) {
						if ( (abr) &&
							   (abr.id) &&
							   (abr.id.length === 10) &&
							   ( (!ztDbTrue(network['private'])) || ztDbTrue(abr['authorized']) ) &&
							   (ztDbTrue(abr['activeBridge'])) ) {
							if (activeBridges.length)
								activeBridges += ',';
							activeBridges += abr.id;
						}
						return nextKey(null);
					});
				},next);
			} else return next(null);
		});

	}],function(err) {

		if (err) {
			console.error('error answering netconf-request for '+peerId.address()+': '+err);
			return;
		}

		var response = new Dictionary();
		response.data['peer'] = peerId.address();
		response.data['nwid'] = nwid;
		response.data['type'] = 'netconf-response';
		response.data['requestId'] = requestId;

		if ((network)&&(authorized)) {
			var certificateOfMembership = null;
			var privateNetwork = ztDbTrue(network['private']);

			async.series([function(next) {

				// Generate certificate of membership if necessary
				if (privateNetwork) {
					generateCertificateOfMembership(nwid,peerId.address(),function(cert,exitCode) {
						if (cert) {
							certificateOfMembership = cert;
							return next(null);
						} else return next(new Error('zerotier-idtool returned '+exitCode));
					});
				} else return next(null);

			}],function(err) {

				// Send response to parent process
				if (err) {
					console.error('unable to generate certificate for peer '+peerId.address()+' on network '+nwid+': '+err);
					response.data['error'] = 'ACCESS_DENIED'; // unable to generate certificate
				} else {
					var netconf = new Dictionary();

					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES] = network['etherTypes'];
					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID] = nwid;
					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP] = Date.now().toString(16);
					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO] = peerId.address();
					if (network['multicastLimit'])
						netconf.data[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT] = network['multicastLimit'];
					if (network['multicastRates']) {
						var ratesD = new Dictionary();
						var ratesJ = JSON.parse(network['multicastRates']);
						for(var k in ratesJ) {
							if ((k)&&(ratesJ[k]))
								ratesD.data[k] = ratesJ[k];
						}
						netconf.data[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES] = ratesD.toString();
					}
					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_PRIVATE] = privateNetwork ? '1' : '0';
					if (network['name'])
						netconf.data[ZT_NETWORKCONFIG_DICT_KEY_NAME] = network['name'];
					if (network['desc'])
						netconf.data[ZT_NETWORKCONFIG_DICT_KEY_DESC] = network['desc'];
					if ((v4NeedAssign)&&(v4Assignments.length > 0))
						netconf.data[ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC] = v4Assignments.join(',');
					if ((v6NeedAssign)&&(v6Assignments.length > 0))
						netconf.data[ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC] = v6Assignments.join(',');
					if (certificateOfMembership !== null)
						netconf.data[ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP] = certificateOfMembership;
					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST] = ztDbTrue(network['enableBroadcast']) ? '1' : '0';
					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING] = ztDbTrue(network['allowPassiveBridging']) ? '1' : '0';
					if ((activeBridges)&&(activeBridges.length > 0))
						netconf.data[ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES] = activeBridges; // comma-delimited list

					response.data['netconf'] = netconf.toString();
				}

				process.stdout.write(response.toString()+'\n');

			});

		} else {

			// Peer not authorized to join network or network not found (right now we always send ACCESS_DENIED)
			response.data['error'] = 'ACCESS_DENIED';
			process.stdout.write(response.toString()+'\n');

		}

	});
}

function handleMessage(dictStr)
{
	var message = new Dictionary(dictStr);
	if (!('type' in message.data)) {
		console.error('ignored message without request type field');
		return;
	} else if (message.data['type'] === 'netconf-init') {
		doNetconfInit(message);
	} else if (message.data['type'] === 'netconf-request') {
		doNetconfRequest(message);
	} else {
		console.error('ignored unrecognized message type: '+message.data['type']);
	}
};

//
// Read stream of double-CR-terminated dictionaries from stdin until close/EOF
//

var stdinReadBuffer = '';

process.stdin.on('readable',function() {
	var chunk = process.stdin.read();
	if (chunk)
		stdinReadBuffer += chunk;
	for(;;) {
		var boundary = stdinReadBuffer.indexOf('\n\n');
		if (boundary >= 0) {
			handleMessage(stdinReadBuffer.substr(0,boundary + 1));
			stdinReadBuffer = stdinReadBuffer.substr(boundary + 2);
		} else break;
	}
});
process.stdin.on('end',function() {
	process.exit(0);
});
process.stdin.on('close',function() {
	process.exit(0);
});
process.stdin.on('error',function() {
	process.exit(0);
});

// Tell ZeroTier One that the service is running, solicit netconf-init
process.stdout.write('type=ready\n\n');

