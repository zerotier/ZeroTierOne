/*
 * ZeroTier One Redis netconf master test script -- see README.md for instructions
 */

// Use this Redis database number to avoid conflicting with running data that might be in DB #0
var redisDatabaseNumber = 9;

// ---------------------------------------------------------------------------
// Test data

// Test signing identity (and first 10 digits of our test network IDs of course)
var netconfMasterIdentity = "39410d7f2a:0:badb041ef2a6e50e001222e76cf41ee41a71d944e67501d71d0635701975bd27a7c84106d76a4b14c5dc0082cce73ecbf05762df9d740df10314ce8f58841c99:ae8cc6d2b48123b48c5c78d4ead5947adc58cbcab2585b3997061e2316a60f7916ac5564e7602e4d035816fd11906d4e0018a18c8fc5fd88b4e46ce706c45597";

// Test member identities
var memberIdentities = {
	"bf521d42f3": "bf521d42f3:0:1f08082079e35dc4a08e87947b9f314ec1cbecd8ff9f744f69a5ba9840efe76d9490a307262f684d643c7ba517dd9209d65b4cf2f691f21c3884514b9dc88fb1:9b8b4a7de3bae5c26513ce1c75495e4a42b7c7b29dc650bea477019f13f57f6b84f098fe3498caf758175dcb0ddafd8fc818b05f7cc620b46a51c78a9177c96a",
	"c0dd8dbaf3": "c0dd8dbaf3:0:ce3513e1344ec32cb5d914cf2295d76a4818a8f6c0e9242efbf9744a861893552d8bed75a4fd9954ca1db259e72fd4e0c3a1f7b1ef665c341ab3c4929e741716:92410fe5b422bb1891a94d139e219a5b035866e9070eb43a96898f71aef61e01dee9e9de8f6e9565d5a1c3167b128a040781bf627d643e6be368d8f32eeaa27e",
	"c8a24c4e66": "c8a24c4e66:0:4b1fea827d3e36bf7aa880ccb31ba9af85c4822da3ee672bebd0d7b6b2e39052de45d185a0093badb4945c9c51bf6274e2fb60405cd5fe92e35af47ada133b71:e78b9df0355eece7b12274c049d96219a3025c0612f87138dd82f171dcad9ef6ac6fdd362e2d81cd79abdfea310a9c8eb9b66bce00c11f8ee90e6a5e09f6de6f",
	"d75952db23": "d75952db23:0:3f94e97ca4f5d406af80d56e2b92dd1218214f683e4828b69f0e3686888b397385979f882e1112ad45b8d913c11968eeaf67bd117bdcc648d903b3caac669284:b5798859e7bcce9ef75024f5bc53ee1299aa05e129dfa528b362f7be7ed4947e5a07634d55e53d96f65967a5819537327eadab2fe93d487348864a3acdbfd06d",
	"46cf921359": "46cf921359:0:5bb72587cd0be9c7ed5fa4a8660251533c9dce8ce9267e6d886eb9670ca2cb44c610bcd37d64d33a1a66c8ece66e79a9528e9ca6d6ee718770551de4ab6a585a:ea5fb0429234a4e4f182d8bedcdacbd896e6502347e79e8ce9cacb6ca9271145457ce797a585511427f7d03bb5add636d408e8292ef3f9a5478da1d6ac56cf90",
	"d3cc6e0998": "d3cc6e0998:0:ab43003195bcbfb52d860eec52bca97782ca7a7d640721dbfa92e2ba0d897e19e61c6e6ca8f55ba3600d3a5163474679c29c1c5873ca3f4a102adbc696e7bf9f:dcfb7c9ec072aca226d86d3d3c9229c3cf1a6ef2ab650e6c0ac314951eb391e8d5dc9751259ece45e8cf0bca3540da82ce984c7a04e0cab7f74ffe15ad81a2db",
	"3b09b12f05": "3b09b12f05:0:413e3a01d5b5b626f815ef2f984caebd2337fd7a679a9c35a1d4fb63e357625bfa9905bfc93f866908baeac0df943f356981ad735dd6125941961f1950145956:9be0be044d2f0625420017fe1dddda1321739ce1072ea9f2af6d651af07342c83302fdfeef80184315625761407084c2da08d2a0e61d9fd297bec46a2fe46fdc",
	"b1df956f1b": "b1df956f1b:0:87635a20426a8b7ed6d590fdc972e0849635f9c765fc1edc05724eb79d32cd4d9962329074f7fd9673acc0dc3898e1f3c720b8aac8a1b382e0ff421810785f94:408c8a3a55d12846e956369c506148c710345dd7249239a10dc2829e79536e864867cadaf48ea9626c7489307a9832fdaef854cff065331696fee28e597586cc"
};

// Test database initial state
var initialTestData = {
	"zt1:schema": 2,

	"zt1:network:39410d7f2a111111:~": {
		"id": "39410d7f2a111111",
		"name": "zerotier-testnet",
		"desc": "Test Public Network",
		"private": "0",
		"etherTypes": "0800,0806",
		"enableBroadcast": "1",
		"v4AssignMode": "zt",
		"v4AssignPool": "192.168.123.0/24",
		"v6AssignMode": "none"
	},
	"zt1:network:39410d7f2a111111:revision": 1,

	"zt1:network:39410d7f2a222222:~": {
		"id": "39410d7f2a222222",
		"name": "zerotier-testnet2",
		"desc": "Test Private Network",
		"private": "1",
		"etherTypes": "0800,0806",
		"enableBroadcast": "1",
		"v4AssignMode": "zt",
		"v4AssignPool": "192.168.124.0/24",
		"v6AssignMode": "none"
	},
	"zt1:network:39410d7f2a222222:members": [ "bf521d42f3", "c0dd8dbaf3" ],
	"zt1:network:39410d7f2a222222:member:bf521d42f3": {
		"id": "bf521d42f3",
		"nwid": "39410d7f2a222222",
		"authorized": "1",
		"identity": "bf521d42f3:0:1f08082079e35dc4a08e87947b9f314ec1cbecd8ff9f744f69a5ba9840efe76d9490a307262f684d643c7ba517dd9209d65b4cf2f691f21c3884514b9dc88fb1"
	},
	"zt1:network:39410d7f2a222222:member:c0dd8dbaf3": {
		"id": "c0dd8dbaf3",
		"nwid": "39410d7f2a222222",
		"authorized": "0",
		"identity": "c0dd8dbaf3:0:ce3513e1344ec32cb5d914cf2295d76a4818a8f6c0e9242efbf9744a861893552d8bed75a4fd9954ca1db259e72fd4e0c3a1f7b1ef665c341ab3c4929e741716"
	},
	"zt1:network:39410d7f2a222222:revision": 2
};

// ---------------------------------------------------------------------------

var async = require('async');
var redis = require('redis');

var DB = redis.createClient();
DB.on("error",function(err) { console.error('redis query error: '+err); });
DB.select(redisDatabaseNumber,function() {});

async.series([function(nextStepInTesting) {

	console.log('Loading test data...');

	async.eachSeries(Object.keys(initialTestData),function(key,next) {
		var value = initialTestData[key];
		if (typeof value === 'object') {
			async.eachSeries(Object.keys(value),function(hkey,next2) {
				DB.hset(key,hkey,value[hkey],next2);
			},next);
		} else if ((typeof value !== 'undefined')&&(value !== null)) {
			if (Array.isArray(value)) {
				DB.sadd(key,value,next);
			} else {
				DB.set(key,value,next);
			}
		} else return next(null);
	},function(err) {
		if (err) {
			console.log('Error loading initial data: '+err);
			return process.exit(1);
		} else {
			return nextStepInTesting();
		}
	});

},function(nextStepInTesting) {

}]);
