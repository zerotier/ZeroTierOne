// ---------------------------------------------------------------------------
// Customizable parameters:

// How frequently in ms to run tests
//var RUN_TEST_EVERY = (60 * 5 * 1000);
var RUN_TEST_EVERY = 1000;

// Maximum test duration in milliseconds (must be less than RUN_TEST_EVERY)
var TEST_DURATION = (60 * 1000);

// Where should I contact to register and query a list of other nodes?
var SERVER_HOST = '174.136.102.178';
var SERVER_PORT = 18080;

// Which port should agents use for their HTTP?
var AGENT_PORT = 18888;

// Payload size in bytes
var PAYLOAD_SIZE = 100000;

// ---------------------------------------------------------------------------

var ipaddr = require('ipaddr.js');
var os = require('os');
var http = require('http');
var async = require('async');

var express = require('express');
var app = express();

// Find our ZeroTier-assigned RFC4193 IPv6 address
var thisAgentId = null;
var interfaces = os.networkInterfaces();
if (!interfaces) {
	console.error('FATAL: os.networkInterfaces() failed.');
	process.exit(1);
}
for(var ifname in interfaces) {
	var ifaddrs = interfaces[ifname];
	if (Array.isArray(ifaddrs)) {
		for(var i=0;i<ifaddrs.length;++i) {
			if (ifaddrs[i].family == 'IPv6') {
				try {
					var ipbytes = ipaddr.parse(ifaddrs[i].address).toByteArray();
					if ((ipbytes.length === 16)&&(ipbytes[0] == 0xfd)&&(ipbytes[9] == 0x99)&&(ipbytes[10] == 0x93)) {
						thisAgentId = '';
						for(var j=0;j<16;++j) {
							var tmp = ipbytes[j].toString(16);
							if (tmp.length === 1)
								thisAgentId += '0';
							thisAgentId += tmp;
						}
					}
				} catch (e) {
					console.error(e);
				}
			}
		}
	}
}
if (thisAgentId === null) {
	console.error('FATAL: no ZeroTier-assigned RFC4193 IPv6 addresses found on any local interface!');
	process.exit(1);
}

//console.log(thisAgentId);

// Create a random (and therefore not very compressable) payload
var payload = new Buffer(PAYLOAD_SIZE);
for(var xx=0;xx<PAYLOAD_SIZE;++xx) {
	payload.writeUInt8(Math.round(Math.random() * 255.0),xx);
}

// Incremented for each test
var testCounter = 0;

function registerAndGetPeers(callback)
{
	http.get({
		host: SERVER_HOST,
		port: SERVER_PORT,
		path: '/'+thisAgentId
	},function(res) {
		var body = '';
		res.on('data',function(chunk) { body += chunk.toString(); });
		res.on('end',function() {
			try {
				var peers = JSON.parse(body);
				if (Array.isArray(peers))
					return callback(null,peers);
				else return callback(new Error('invalid JSON response from server'),null);
			} catch (e) {
				return callback(new Error('invalid JSON response from server'),null);
			}
		});
	}).on('error',function(e) {
		return callback(e,null);
	});
};

function performTestOnAllPeers(peers,callback)
{
	var allResults = {};
	var timedOut = false;
	var endOfTestTimer = setTimeout(function() {
		timedOut = true;
		return callback(allResults);
	},TEST_DURATION);
	var testStartTime = Date.now();

	async.each(peers,function(peer,next) {
		if (timedOut)
			return next(null);
		if (peer.length !== 32)
			return next(null);

		var connectionStartTime = Date.now();
		allResults[peer] = {
			testStart: testStartTime,
			start: connectionStartTime,
			end: null,
			error: null,
			bytes: 0,
			test: testCounter
		};

		var peerHost = '';
		peerHost += peer.substr(0,4);
		peerHost += ':';
		peerHost += peer.substr(4,4);
		peerHost += ':';
		peerHost += peer.substr(8,4);
		peerHost += ':';
		peerHost += peer.substr(12,4);
		peerHost += ':';
		peerHost += peer.substr(16,4);
		peerHost += ':';
		peerHost += peer.substr(20,4);
		peerHost += ':';
		peerHost += peer.substr(24,4);
		peerHost += ':';
		peerHost += peer.substr(28,4);

		http.get({
			host: peerHost,
			port: AGENT_PORT,
			path: '/'
		},function(res) {
			var bytes = 0;
			res.on('data',function(chunk) {
				bytes += chunk.length;
			});
			res.on('end',function() {
				if (timedOut)
					return next(null);
				allResults[peer] = {
					testStart: testStartTime,
					start: connectionStartTime,
					end: Date.now(),
					error: null,
					bytes: bytes,
					test: testCounter
				};
				return next(null);
			});
		}).on('error',function(e) {
			if (timedOut)
				return next(null);
			allResults[peer] = {
				testStart: testStartTime,
				start: connectionStartTime,
				end: Date.now(),
				error: e.toString(),
				bytes: 0,
				test: testCounter
			};
			return next(null);
		});
	},function(err) {
		if (!timedOut) {
			clearTimeout(endOfTestTimer);
			return callback(allResults);
		}
	});
};

// Agents just serve up a test payload
app.get('/',function(req,res) {
	return res.status(200).send(payload);
});

var expressServer = app.listen(AGENT_PORT,function () {
	registerAndGetPeers(function(err,peers) {
		if (err) {
			console.error('FATAL: unable to contact or query server: '+err.toString());
			process.exit(1);
		}

		setInterval(function() {
			++testCounter;

			registerAndGetPeers(function(err,peers) {
				if (err) {
					console.error('WARNING: unable to contact or query server, test aborted: '+err.toString());
					return;
				}

				performTestOnAllPeers(peers,function(results) {
					//console.log(results);

					var submit = http.request({
						host: SERVER_HOST,
						port: SERVER_PORT,
						path: '/'+thisAgentId,
						method: 'POST'
					},function(res) {
					}).on('error',function(e) {
						console.error('WARNING: unable to submit results to server: '+err.toString());
					});
					submit.write(JSON.stringify(results));
					submit.end();
				});
			});
		},RUN_TEST_EVERY);
	});
});
