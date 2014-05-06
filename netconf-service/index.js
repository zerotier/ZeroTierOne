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

// Path to zerotier-idtool
var ZEROTIER_IDTOOL = '/usr/local/bin/zerotier-idtool';

// Connect to redis, assuming database 0 and no auth (for now)
var redis = require('redis');
var DB = redis.createClient();
DB.on("error",function(err) {
	console.error('Redis query error: '+err);
});

// Encoding-compatible with Dictionary.hpp in ZeroTier One C++ code base
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

/* Dictionary tester
var testDict1 = new Dictionary();
var testDict2 = new Dictionary();
testDict1.data['foo'] = '1';
testDict1.data['bar'] = 'The quick brown fox\ncontained a carriage return.';
testDict2.data['embeddedDictionary'] = testDict1.toString();
testDict2.data['baz'] = 'eklrjelkrnlqkejrnlkqerne';
console.log(testDict2.toString());
console.log('After fromString(toString())...\n');
console.log((new Dictionary(testDict2.toString())).toString());
process.exit(0);
*/

// Variables initialized by netconf-init message
var netconfSigningIdentity = null;

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

	this.hasSecretKey = function() {
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
	var response = new Dictionary();

	if (!('type' in message.data))
		return; // no request type

	if (message.data['type'] === 'netconf-init') {
		netconfSigningIdentity = new Identity(message.data['netconfId']);
		if (!netconfSigningIdentity.isValid())
			netconfSigningIdentity = null; // empty strings and such are not valid
		return; // no response expected
	} else if (message.data['type'] === 'netconf-request') {
		// Get required fields
		var peerId = new Identity(message.data['peerId']);
		var fromIpAndPort = message.data['from'];
		var nwid = message.data['nwid'];
		var requestId = message.data['requestId'];
		if ((!peerId)||(!peerId.isValid())||(!fromIpAndPort)||(!nwid)||(nwid.length !== 16)||(!requestId))
			return;

		// Get optional fields
		var meta = new Dictionary(message.data['meta']);
	} else return;

	process.stdout.write(response.toString()+'\n');
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
