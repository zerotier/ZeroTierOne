#!/usr/bin/env node

// Note: this is unfinished and not currently used. Stashed in case we resurrect this idea.

var UDP_PORT_START = 9994;
var UDP_PORT_COUNT = 16384;
var HTTP_PORT = 8080;
var LONG_POLLING_TIMEOUT = 25000;

var http = require('http');
var dgram = require('dgram');

// clients[token] = [ most recent HTTP activity, assigned UDP socket ]
var clients = {};

// GETs[token] = [ [ request, timestamp ], ... ]
var GETs = {};

// mappings[localPort+'/'+remoteIp+'/'+remotePort] = { ZT source: [ token ] }
var mappings = {};

// Array of available UDP sockets to assign randomly to clients
var udpSocketPool = [];

function onIncomingUdp(socket,message,remoteIp,remotePort)
{
	if (message.length > 16) {
		var mappingKey = socket.localPort + '/' + remoteIp + '/' + remotePort;
		var mapping = mappings[mappingKey];
		if (mapping) {
			var ztDestination = message.readUIntBE(8,5);
			if (ztDestination in mapping) {
			}
		}
	}
}

function onOutgoingUdp(token,socket,message,remoteIp,remotePort)
{
	if (message.length > 16) {
		var ztDestination = message.readUIntBE(8,5);
		var ztSource = (message.length >= 28) ? message.readUIntBE(13,5) ? 0;
		if ((ztSource & 0xff00000000) == 0xff00000000) // fragment
			ztSource = 0;

		if ((ztDestination !== 0)&&((ztDestination & 0xff00000000) !== 0xff00000000)) {
			socket.send(message,0,message.length,remotePort,remoteIp);
		}
	}
}

function doHousekeeping()
{
}

for(var udpPort=UDP_PORT_START;udpPort<(UDP_PORT_START+UDP_PORT_COUNT)++udpPort) {
	var socket = dgram.createSocket('udp4',function(message,rinfo) { onIncomingUdp(socket,message,rinfo.address,rinfo.port); });
	socket.on('listening',function() {
		console.log('Listening on '+socket.localPort);
		udpSocketPool.push(socket);
	}
	socket.on('error',function() {
		console.log('Error listening on '+socket.localPort);
		socket.close();
	})
	socket.bind(udpPort);
}

server = http.createServer(function(request,response) {
	console.log(request.socket.remoteAddress+" "+request.method+" "+request.url);

	try {
		// /<proxy token>/<ignored>/...
		var urlSp = request.url.split('/');
		if ((urlSp.length >= 3)&&(udpSocketPool.length > 0)) {
			var token = urlSp[1]; // urlSp[0] == '' since URLs start with /

			if (token.length >= 8) {
				var client = clients[token];
				if (!Array.isArray(client)) {
					client = [ Date.now(),udpSocketPool[Math.floor(Math.random() * udpSocketPool.length)] ];
					clients[token] = client;
				} else client[0] = Date.now();

				if (request.method === "GET") {

					// /<proxy token>/<ignored> ... waits via old skool long polling

				} else if (request.method === "POST") {

					// /<proxy token>/<ignored>/<dest ip>/<dest port>
					if (urlSp.length === 5) {
						var ipSp = urlSp[3].split('.');
						var port = parseInt(urlSp[4],10);
						// Note: do not allow the use of this proxy to talk to privileged ports
						if ((ipSp.length === 4)&&(port >= 1024)&&(port <= 0xffff)) {
							var ip = [ parseInt(ipSp[0]),parseInt(ipSp[1]),parseInt(ipSp[2]),parseInt(ipSp[3]) ];
							if (   (ip[0] > 0)
							     &&(ip[0] < 240)
							     &&(ip[0] !== 127)
							     &&(ip[1] >= 0)
							     &&(ip[1] <= 255)
							     &&(ip[2] >= 0)
							     &&(ip[2] <= 255)
							     &&(ip[3] > 0)
							     &&(ip[3] < 255) ) {
								var postData = null;
								request.on('data',function(chunk) {
									postData = ((postData === null) ? chunk : Buffer.concat([ postData,chunk ]));
								});
								request.on('end',function() {
									if (postData !== null)
										onOutgoingUdp(token,client[1],postData,urlSp[3],port);
									response.writeHead(200,{'Content-Length':0,'Pragma':'no-cache','Cache-Control':'no-cache'});
									response.end();
								});
								return; // no 400 -- read from stream
							} // else 400
						} // else 400
					} // else 400

				} // else 400

			} // else 400
		} // else 400
	} catch (e) {} // 400

	response.writeHead(400,{'Content-Length':0,'Pragma':'no-cache','Cache-Control':'no-cache'});
	response.end();
	return;
});

setInterval(doHousekeeping,5000);

server.setTimeout(120000);
server.listen(HTTP_PORT);
