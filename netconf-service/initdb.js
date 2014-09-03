/*
 * Populates a new Redis database with data, which can be edited below.
 */

var INIT_DATA = {
	// Must be present in any database
	"zt1": 1,

	/* The network ID here must be set to the ZeroTier address of your netconf
	 * master (the node where netconf-master will be running) plus an arbitrary
	 * 24-bit network ID. This will create the full 16-digit network ID of the
	 * network you will join. This must be in the object name and in the "id"
	 * field within the object itself. */
	"zt1:network:ffffffffff111111:~": {
		"id": "ffffffffff111111",     // netconf master ZT address + 24-bit ID
		"name": "zerotier-testnet",   // short name, no spaces or special chars
		"desc": "Test Network",       // description
		"infrastructure": 0,          // unused by netconf-master
		"private": 0,                 // set to '1' to require member approval
		"creationTime": 0,            // unuxed by netconf-master
		"owner": "",                  // unused by netconf-master
		"etherTypes": "0800,0806",    // hex ethernet frame types allowed
		"enableBroadcast": 1,         // set to '1' to enable ff:ff:ff:ff:ff:ff
		"v4AssignMode": "zt",         // 'zt' to assign, 'none' to let OS do it
		"v4AssignPool": "28.0.0.0/7", // IPv4 net block / netmask bits
		"v6AssignMode": "none"        // 'zt' to assign, 'none' to let OS do it
	}
};

var config = require('./config.js');

config.redisDb = 2;

var async = require('async');
var redis = require('redis');
var DB = redis.createClient();
DB.on("error",function(err) { console.error('redis query error: '+err); });
DB.select(config.redisDb,function() {});

DB.get("zt1",function(err,value) {
	if ((value)&&(!err)) {
		console.log("Redis database #"+config.redisDb+" appears to already contain data; flush it first!");
		return process.exit(0);
	}

	async.eachSeries(Object.keys(INIT_DATA),function(key,next) {
		var value = INIT_DATA[key];
		if (typeof value === 'object') {
			console.log(key);
			async.eachSeries(Object.keys(value),function(hkey,next2) {
				var hvalue = value[hkey];
				if (hvalue === true)
					hvalue = 1;
				if (hvalue === false)
					hvalue = 0;
				if (typeof hvalue !== 'string')
					hvalue = hvalue.toString();
				console.log('\t'+hkey+': '+hvalue);
				DB.hset(key,hkey,hvalue,next2);
			},next);
		} else if ((typeof value !== 'undefined')&&(value !== null)) {
			if (value === true)
				value = 1;
			if (value === false)
				value = 0;
			if (typeof value !== 'string')
				value = value.toString();
			console.log(key+': '+value);
			DB.set(key,value,next);
		} else return next(null);
	},function(err) {
		console.log('Done!');
		return process.exit(0);
	});
});
