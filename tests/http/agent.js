// ZeroTier distributed HTTP test agent

// ---------------------------------------------------------------------------
// Customizable parameters:

// Time between startup and first test attempt
var TEST_STARTUP_LAG = 10000;

// Maximum interval between test attempts (actual timing is random % this)
var TEST_INTERVAL_MAX = (60000 * 10);

// Test timeout in ms
var TEST_TIMEOUT = 30000;

// Where should I get other agents' IDs and POST results?
var SERVER_HOST = '52.26.196.147';
var SERVER_PORT = 18080;

// Which port do agents use to serve up test data to each other?
var AGENT_PORT = 18888;

// Payload size in bytes
var PAYLOAD_SIZE = 5000;

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

function agentIdToIp(agentId)
{
	var ip = '';
	ip += agentId.substr(0,4);
	ip += ':';
	ip += agentId.substr(4,4);
	ip += ':';
	ip += agentId.substr(8,4);
	ip += ':';
	ip += agentId.substr(12,4);
	ip += ':';
	ip += agentId.substr(16,4);
	ip += ':';
	ip += agentId.substr(20,4);
	ip += ':';
	ip += agentId.substr(24,4);
	ip += ':';
	ip += agentId.substr(28,4);
	return ip;
};

var lastTestResult = null;
var allOtherAgents = {};

function doTest()
{
	var submit = http.request({
		host: SERVER_HOST,
		port: SERVER_PORT,
		path: '/'+thisAgentId,
		method: 'POST'
	},function(res) {
		var body = '';
		res.on('data',function(chunk) { body += chunk.toString(); });
		res.on('end',function() {

			if (body) {
				try {
					var peers = JSON.parse(body);
					if (Array.isArray(peers)) {
						for(var xx=0;xx<peers.length;++xx)
							allOtherAgents[peers[xx]] = true;
					}
				} catch (e) {}
			}

			var agents = Object.keys(allOtherAgents);
			if (agents.length > 1) {

				var target = agents[Math.floor(Math.random() * agents.length)];
				while (target === thisAgentId)
					target = agents[Math.floor(Math.random() * agents.length)];

				var testRequest = null;
				var timeoutId = null;
				timeoutId = setTimeout(function() {
					if (testRequest !== null)
						testRequest.abort();
					timeoutId = null;
				},TEST_TIMEOUT);
				var startTime = Date.now();

				testRequest = http.get({
					host: agentIdToIp(target),
					port: AGENT_PORT,
					path: '/'
				},function(res) {
					var bytes = 0;
					res.on('data',function(chunk) { bytes += chunk.length; });
					res.on('end',function() {
						lastTestResult = {
							source: thisAgentId,
							target: target,
							time: (Date.now() - startTime),
							bytes: bytes,
							timedOut: (timeoutId === null),
							error: null
						};
						if (timeoutId !== null)
							clearTimeout(timeoutId);
						return setTimeout(doTest,Math.round(Math.random() * TEST_INTERVAL_MAX) + 1);
					});
				}).on('error',function(e) {
					lastTestResult = {
						source: thisAgentId,
						target: target,
						time: (Date.now() - startTime),
						bytes: 0,
						timedOut: (timeoutId === null),
						error: e.toString()
					};
					if (timeoutId !== null)
						clearTimeout(timeoutId);
					return setTimeout(doTest,Math.round(Math.random() * TEST_INTERVAL_MAX) + 1);
				});

			} else {
				return setTimeout(doTest,1000);
			}

		});
	}).on('error',function(e) {
		console.log('POST failed: '+e.toString());
		return setTimeout(doTest,1000);
	});
	if (lastTestResult !== null) {
		submit.write(JSON.stringify(lastTestResult));
		lastTestResult = null;
	}
	submit.end();
};

// Agents just serve up a test payload
app.get('/',function(req,res) { return res.status(200).send(payload); });

var expressServer = app.listen(AGENT_PORT,function () {
	// Start timeout-based loop
	setTimeout(doTest(),TEST_STARTUP_LAG);
});
