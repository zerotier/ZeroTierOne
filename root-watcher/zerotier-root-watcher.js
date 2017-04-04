'use strict';

const pg = require('pg');
const zlib = require('zlib');
const http = require('http');
const fs = require('fs');
const async = require('async');

const config = JSON.parse(fs.readFileSync('./config.json'));
const roots = config.roots||{};

const db = new pg.Pool(config.db);

process.on('uncaughtException',function(err) {
	console.error('ERROR: uncaught exception: '+err);
	if (err.stack)
		console.error(err.stack);
});

function httpRequest(host,port,authToken,method,path,args,callback)
{
	var responseBody = [];
	var postData = (args) ? JSON.stringify(args) : null;

	var req = http.request({
		host: host,
		port: port,
		path: path,
		method: method,
		headers: {
			'X-ZT1-Auth': (authToken||''),
			'Content-Length': (postData) ? postData.length : 0
		}
	},function(res) {
		res.on('data',function(chunk) {
			if ((chunk)&&(chunk.length > 0))
				responseBody.push(chunk);
		});
		res.on('timeout',function() {
			try {
				if (typeof callback === 'function') {
					var cb = callback;
					callback = null;
					cb(new Error('connection timed out'),null);
				}
				req.abort();
			} catch (e) {}
		});
		res.on('error',function(e) {
			try {
				if (typeof callback === 'function') {
					var cb = callback;
					callback = null;
					cb(new Error('connection timed out'),null);
				}
				req.abort();
			} catch (e) {}
		});
		res.on('end',function() {
			if (typeof callback === 'function') {
				var cb = callback;
				callback = null;
				if (responseBody.length === 0) {
					return cb(null,{});
				} else {
					responseBody = Buffer.concat(responseBody);

					if (responseBody.length < 2) {
						return cb(null,{});
					}

					if ((responseBody.readUInt8(0,true) === 0x1f)&&(responseBody.readUInt8(1,true) === 0x8b)) {
						try {
							responseBody = zlib.gunzipSync(responseBody);
						} catch (e) {
							return cb(e,null);
						}
					}

					try {
						return cb(null,JSON.parse(responseBody));
					} catch (e) { 
						return cb(e,null);
					}
				}
			}
		});
	}).on('error',function(e) {
		try {
			if (typeof callback === 'function') {
				var cb = callback;
				callback = null;
				cb(e,null);
			}
			req.abort();
		} catch (e) {}
	}).on('timeout',function() {
		try {
			if (typeof callback === 'function') {
				var cb = callback;
				callback = null;
				cb(new Error('connection timed out'),null);
			}
			req.abort();
		} catch (e) {}
	});

	req.setTimeout(30000);
	req.setNoDelay(true);

	if (postData !== null)
		req.end(postData);
	else req.end();
};

var peerStatus = {};

function saveToDb()
{
	db.connect(function(err,client,clientDone) {
		if (err) {
			console.log('WARNING: database error writing peers: '+err.toString());
			clientDone();
			return setTimeout(saveToDb,config.dbSaveInterval||60000);
		}
		client.query('BEGIN',function(err) {
			if (err) {
				console.log('WARNING: database error writing peers: '+err.toString());
				clientDone();
				return setTimeout(saveToDb,config.dbSaveInterval||60000);
			}
			let timeout = Date.now() - (config.peerTimeout||600000);
			let wtotal = 0;
			async.eachSeries(Object.keys(peerStatus),function(address,nextAddress) {
				let s = peerStatus[address];
				if (s[1] <= timeout) {
					delete peerStatus[address];
					return process.nextTick(nextAddress);
				} else {
					++wtotal;
					client.query('INSERT INTO "Peer" ("ztAddress","timestamp","versionMajor","versionMinor","versionRev","rootId","phyPort","phyLinkQuality","phyLastReceive","phyAddress") VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10)',s,nextAddress);
				}
			},function(err) {
				if (err)
					console.log('WARNING database error writing peers: '+err.toString());
				console.log(Date.now().toString()+' '+wtotal);
				client.query('COMMIT',function(err,result) {
					clientDone();
					return setTimeout(saveToDb,config.dbSaveInterval||60000);
				});
			});
		});
	});
};

function doRootUpdate(name,id,ip,port,peersPath,authToken,interval)
{
	httpRequest(ip,port,authToken,"GET",peersPath,null,function(err,res) {
		if (err) {
			console.log('WARNING: cannot reach '+name+peersPath+' (will try again in 1s): '+err.toString());
			return setTimeout(function() { doRootUpdate(name,id,ip,port,peersPath,authToken,interval); },1000);
		}
		if (!Array.isArray(res)) {
			console.log('WARNING: cannot reach '+name+peersPath+' (will try again in 1s): response is not an array of peers');
			return setTimeout(function() { doRootUpdate(name,id,ip,port,peersPath,authToken,interval); },1000);
		}

		//console.log(name+': '+res.length+' peer entries.');
		let now = Date.now();
		let count = 0;
		for(let pi=0;pi<res.length;++pi) {
			let peer = res[pi];
			let address = peer.address;
			let ztAddress = parseInt(address,16)||0;
			if (!ztAddress)
				continue;

			let paths = peer.paths;
			if ((Array.isArray(paths))&&(paths.length > 0)) {
				let bestPath = null;
				for(let i=0;i<paths.length;++i) {
					if (paths[i].active) {
						let lr = paths[i].lastReceive;
						if ((lr > 0)&&((!bestPath)||(bestPath.lastReceive < lr)))
							bestPath = paths[i];
					}
				}

				if (bestPath) {
					let a = bestPath.address;
					if (typeof a === 'string') {
						let a2 = a.split('/');
						if (a2.length === 2) {
							let vmaj = peer.versionMajor;
							if ((typeof vmaj === 'undefined')||(vmaj === null)) vmaj = -1;
							let vmin = peer.versionMinor;
							if ((typeof vmin === 'undefined')||(vmin === null)) vmin = -1;
							let vrev = peer.versionRev;
							if ((typeof vrev === 'undefined')||(vrev === null)) vrev = -1;
							let lr = parseInt(bestPath.lastReceive)||0;

							let s = peerStatus[address];
							if ((!s)||(s[8] < lr)) {
								peerStatus[address] = [
									ztAddress,
									now,
									vmaj,
									vmin,
									vrev,
									id,
									parseInt(a2[1])||0,
									parseFloat(bestPath.linkQuality)||1.0,
									lr,
									a2[0]
								];
							}
							++count;
						}
					}
				}
			}
		}

		console.log(name+': '+count+' peers with active direct paths.');
		return setTimeout(function() { doRootUpdate(name,id,ip,port,peersPath,authToken,interval); },interval);
	});
};

for(var r in roots) {
	var rr = roots[r];
	if (rr.peers)
		doRootUpdate(r,rr.id,rr.ip,rr.port,rr.peers,rr.authToken||null,config.interval||60000);
}

return setTimeout(saveToDb,config.dbSaveInterval||60000);
