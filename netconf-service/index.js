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

// Fields in netconf response dictionary
var ZT_NETWORKCONFIG_DICT_KEY_NETCONF_SERVICE_VERSION = "ncver";
var ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES = "et";
var ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID = "nwid";
var ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP = "ts";
var ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO = "id";
var ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_PREFIX_BITS = "mpb";
var ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_DEPTH = "md";
var ZT_NETWORKCONFIG_DICT_KEY_ARP_CACHE_TTL = "cARP";
var ZT_NETWORKCONFIG_DICT_KEY_NDP_CACHE_TTL = "cNDP";
var ZT_NETWORKCONFIG_DICT_KEY_EMULATE_ARP = "eARP";
var ZT_NETWORKCONFIG_DICT_KEY_EMULATE_NDP = "eNDP";
var ZT_NETWORKCONFIG_DICT_KEY_IS_OPEN = "o";
var ZT_NETWORKCONFIG_DICT_KEY_NAME = "name";
var ZT_NETWORKCONFIG_DICT_KEY_DESC = "desc";
var ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC = "v4s";
var ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC = "v6s";
var ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES = "mr";
var ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP = "com";

// Path to zerotier-idtool binary, invoked to enerate certificates of membership
var ZEROTIER_IDTOOL = '/usr/local/bin/zerotier-idtool';

// Connect to redis, assuming database 0 and no auth (for now)
var redis = require('redis');
var DB = redis.createClient();
DB.on("error",function(err) {
	console.error('redis query error: '+err);
});

// Global variables -- these are initialized on startup or netconf-init message
var netconfSigningIdentity = null; // identity of netconf master, with private key portion

function ztDbTrue(v) { return ((v === '1')||(v === 'true')||(v > 0)); }
function csvToArray(csv) { return (((typeof csv === 'string')&&(csv.length > 0)) ? csv.split(',') : []); }
function arrayToCsv(a) { return ((Array.isArray(a)) ? ((a.length > 0) ? a.join(',') : '') : (((a !== null)&&(typeof a !== 'undefined')) ? a.toString() : '')); }

//
// ZeroTier One Dictionary -- encoding-compatible with Dictionary in C++ code base
//

function Dictionary(fromStr)
{
	var thiz = this;

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

		for(var key in thiz.data) {
			str += thiz._esc(key);
			str += '=';
			var value = thiz.data[key];
			if (value)
				str += thiz._esc(value.toString());
			str += '\n';
		}

		return str;
	};

	this.fromString = function(str) {
		thiz.data = {};
		if (typeof str !== 'string')
			return thiz;

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

			var k = thiz._unesc(lines[l].substr(0,eqAt));
			++eqAt;
			if ((k)&&(k.length > 0))
				thiz.data[k] = thiz._unesc((eqAt < lines[l].length) ? lines[l].substr(eqAt) : '');
		}

		return thiz;
	};

	if ((typeof fromStr === 'string')&&(fromStr.length > 0))
		thiz.fromString(fromStr);
};

//
// Identity implementation using zerotier-idtool as subprocess to do actual crypto work
//

function Identity(idstr)
{
	var thiz = this;

	this.str = '';
	this.fields = [];

	this.toString = function() {
		return thiz.str;
	};

	this.address = function() {
		return ((thiz.fields.length > 0) ? thiz.fields[0] : '0000000000');
	};

	this.fromString = function(str) {
		thiz.str = '';
		thiz.fields = [];
		if (typeof str !== 'string')
			return;
		for(var i=0;i<str.length;++i) {
			if ("0123456789abcdef:ABCDEF".indexOf(str.charAt(i)) < 0)
				return; // invalid character in identity
		}
		var fields = str.split(':');
		if ((fields.length < 3)||(fields[0].length !== 10)||(fields[1] !== '0'))
			return;
		thiz.fields = fields;
	};

	this.isValid = function() {
		if ((thiz.fields.length < 3)||(thiz.fields[0].length !== 10)||(thiz.fields[1] !== '0'))
			return true;
		return false;
	};

	this.hasPrivate = function() {
		return ((thiz.isValid())&&(thiz.fields.length >= 4));
	};

	if (typeof idstr === 'string')
		thiz.fromString(idstr);
};

//
// Message handler for messages over ZeroTier One service bus
//

function handleMessage(dictStr)
{
	var message = new Dictionary(dictStr);

	if (!('type' in message.data)) {
		console.error('ignored message without request type field');
		return;
	}

	if (message.data['type'] === 'netconf-init') {

		netconfSigningIdentity = new Identity(message.data['netconfId']);
		if (!netconfSigningIdentity.hasPrivate()) {
			netconfSigningIdentity = null;
			console.error('got invalid netconf signing identity');
		}

	} else if (message.data['type'] === 'netconf-request') {
		if ((!netconfSigningIdentity)||(!netconfSigningIdentity.hasPrivate())) {
			console.error('got netconf-request before netconf-init, ignored');
			return;
		}

		// Get required fields
		var peerId = new Identity(message.data['peerId']);
		var fromIpAndPort = message.data['from'];
		var nwid = message.data['nwid'];
		var requestId = message.data['requestId'];
		if ((!peerId)||(!peerId.isValid())||(!fromIpAndPort)||(!nwid)||(nwid.length !== 16)||(!requestId))
			return;

		// Get optional fields
		var meta = new Dictionary(message.data['meta']);
		var clientVersion = message.data['clientVersion'];
		var clientOs = message.data['clientOs'];

		var network = null;
		var member = null;
		var authorized = false;
		var v4NeedAssign = false;
		var v6NeedAssign = false;
		var v4Assignments = [];
		var v6Assignments = [];

		async.series([function(next) { // network lookup
			DB.hgetall('zt1:network:'+nwid+':~',function(err,obj) {
				network = obj;
				return next(err);
			});
		},function(next) { // member record lookup, unless public network
			if ((!network)||(!('nwid' in network)||(network['nwid'] !== nwid))
				return next(null);
			var memberKey = 'zt1:network:'+nwid+':member:'+peerId.address()+':~';
			DB.hgetall(memberKey,function(err,obj) {
				if (err)
					return next(err);
				else if (obj) {
					// Update member object
					member = obj;
					authorized = (ztDbTrue(network['private']) || ztDbTrue(member['authorized']));
					DB.hmset(memberKey,{
						'lastSeen': Date.now(),
						'lastAt': fromIpAndPort,
						'clientVersion': (clientVersion) ? clientVersion : '?.?.?',
						'clientOs': (clientOs) ? clientOs : '?'
					},next);
				} else {
					// Add member object for new and unauthorized member
					authorized = false;
					member = {
						'id': peerId.address(),
						'nwid': nwid,
						'authorized': 0,
						'identity': peerId.toString(),
						'firstSeen': Date.now(),
						'lastSeen': Date.now(),
						'lastAt': fromIpAndPort,
						'clientVersion': (clientVersion) ? clientVersion : '?.?.?',
						'clientOs': (clientOs) ? clientOs : '?'
					};
					DB.hmset(memberKey,member,next);
				}
			});
		},function(next) { // IP address auto-assignment, if needed
			if (!authorized)
				return next(null);

			v4NeedAssign = (network['v4AssignMode'] === 'zt');
			v6NeedAssign = (network['v6AssignMode'] === 'zt');

			var ipa = csvToArray(member['ipAssignments']);
			for(var i=0;i<ipa.length;++i) {
				if ((ipa[i].indexOf('.') > 0)&&(v4NeedAssign))
					v4Assignments.push(ipa[i]);
				else if ((ipa[i].indexOf(':') > 0)&&(v6NeedAssign))
					v6Assignments.push(ipa[i]);
			}

			return next(null);
		},function(next) { // assign IPv4 if needed
			if ((!authorized)||(!v4NeedAssign))
				return next(null);

			var ipAssignmentAttempts = 0; // for sanity-checking
			var v4pool = network['v4AssignPool'];
			var ztaddr = peerId.address();

			var network = 0;
			var netmask = 0;
			var netmaskBits = 0;
			if (v4pool) {
				var v4poolSplit = v4Pool.split('/');
				if (v4poolSplit.length === 2) {
					var networkSplit = v4poolSplit[0].split('.');
					if (networkSplit.length === 4) {
						network |= (parseInt(networkSplit[0],10) << 24) & 0xff000000;
						network |= (parseInt(networkSplit[1],10) << 16) & 0x00ff0000;
						network |= (parseInt(networkSplit[2],10) << 8) & 0x0000ff00;
						network |= parseInt(networkSplit[3],10) & 0x000000ff;
						netmaskBits = parseInt(v4poolSplit[1],10);
						if (netmaskBits > 32)
							netmaskBits = 32; // sanity check
						for(var i=0;i<netmaskBits;++i)
							netmask |= (0x80000000 >> i);
					}
				}
			}
			var invmask = netmask ^ 0xffffffff;
			var abcd = 0;
			var assignment = null;

			var ipAssignmentsKey = 'zt1:network:'+nwid+':ipAssignments';
			var memberKey = 'zt1:network:'+nwid+':member:'+ztaddr+':~';

			async.whilst(
				function() { return ((v4NeedAssign)&&(v4Assignments.length === 0)&&(network !== 0)&&(netmask !== 0xffffffff)&&(ipAssignmentAttempts < 1000)); },
				function(next2) {
					++ipAssignmentAttempts;

					// Generate or increment IP address
					if (abcd === 0) {
						var a = parseInt(ztaddr.substr(2,2),16) & 0xff;
						var b = parseInt(ztaddr.substr(4,2),16) & 0xff;
						var c = parseInt(ztaddr.substr(6,2),16) & 0xff;
						var d = parseInt(ztaddr.substr(8,2),16) & 0xff;
						abcd = (a << 24) | (b << 16) | (c << 8) | d;
					} else ++abcd;
					if ((abcd & 0xff) === 0)
						abcd |= 1;

					// Derive an IP to test and generate assignment ip/bits string
					var ip = (abcd & invmask) | (network & netmask);
					assignment = ((ip >> 24) & 0xff).toString(10) + '.' + ((ip >> 16) & 0xff).toString(10) + '.' + ((ip >> 8) & 0xff).toString(10) + '.' + (ip & 0xff).toString(10) + '/' + netmaskBits.toString(10);

					DB.hget(ipAssignmentsKey,assignment,function(err,value) {
						if (err)
							return next2(err);
						if ((value)&&(value !== ztaddr))
							return next2(null); // if someone's already got this IP, keep looking

						v4Assignments.push(assignment);

						// Save assignment to :ipAssignments hash
						DB.hset(ipAssignmentsKey,assignment,ztaddr,function(err) {
							if (err)
								return next2(err);

							// Save updated CSV list of assignments to member record
							var ipAssignments = member['ipAssignments'];
							if (!ipAssignments)
								ipAssignments = '';
							if (ipAssignments.length > 0)
								ipAssignments += ',';
							ipAssignments += assignment;
							member['ipAssignments'] = ipAssignments;
							DB.hset(memberKey,'ipAssignments',ipAssignments,next2);
						});
					});
				},
				next
			);

		},function(next) { // assign IPv6 if needed -- TODO
			if ((!authorized)||(!v6NeedAssign))
				return next(null);

			return next(null);
		}],function(err) {
			if (err) {
				console.log('error composing response for '+peerId.address()+': '+err);
				return;
			} else if (authorized) {
				// TODO: COM!!!
				var certificateOfMembership = null;

				var netconf = new Dictionary();
				netconf.data[ZT_NETWORKCONFIG_DICT_KEY_NETCONF_SERVICE_VERSION] = '0.0.0';
				netconf.data[ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES] = network['etherTypes'];
				netconf.data[ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID] = nwid;
				netconf.data[ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP] = Date.now().toString();
				netconf.data[ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO] = peerId.address();
				//netconf.data[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_PREFIX_BITS] = 0;
				//netconf.data[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_DEPTH] = 0;
				//netconf.data[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES] = '';
				//netconf.data[ZT_NETWORKCONFIG_DICT_KEY_ARP_CACHE_TTL] = 0;
				//netconf.data[ZT_NETWORKCONFIG_DICT_KEY_NDP_CACHE_TTL] = 0;
				netconf.data[ZT_NETWORKCONFIG_DICT_KEY_EMULATE_ARP] = '0';
				netconf.data[ZT_NETWORKCONFIG_DICT_KEY_EMULATE_NDP] = '0';
				netconf.data[ZT_NETWORKCONFIG_DICT_KEY_IS_OPEN] = ztDbTrue(network['private']) ? '0' : '1';
				netconf.data[ZT_NETWORKCONFIG_DICT_KEY_NAME] = network['name'];
				if (network['desc'])
					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_DESC] = network['desc'];
				if (v4NeedAssign)
					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC] = (v4Assignments.length > 0) ? v4Assignments.join(',') : '';
				if (v6NeedAssign)
					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC] = (v6Assignments.length > 0) ? v6Assignments.join(',') : '';
				if (certificateOfMembership !== null)
					netconf.data[ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP] = certificateOfMembership;

				var response = new Dictionary();
				response.data['peer'] = peerId.address();
				response.data['nwid'] = nwid;
				response.data['type'] = 'netconf-response';
				response.data['requestId'] = requestId;
				response.data['netconf'] = netconf.toString();

				process.stdout.write(response.toString()+'\n');
				return;
			} else {
			}
		});
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
	if ((stdinReadBuffer.length >= 2)&&(stdinReadBuffer.substr(stdinReadBuffer.length - 2) === '\n\n')) {
		handleMessage(stdinReadBuffer);
		stdinReadBuffer = '';
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
