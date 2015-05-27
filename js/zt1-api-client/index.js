'use strict'

var request = require('request');
var constrainTypes = require('./constrain-types.js');

// Types that fields must be in submissions -- used with constrainTypes to
// ensure that submitted JSON objects are correctly typed since the JSON
// API is very sensitive to this. This only includes writable fields since
// non-writable and unknown fields are ignored.
var REQUEST_TYPE_MAPS = {
	'controller/network/*/relay': {
		'address': 'string',
		'phyAddress': 'string'
	},
	'controller/network/*/rule': {
		'ruleId': 'integer',
		'nodeId': 'string',
		'vlanId': 'integer',
		'vlanPcp': 'integer',
		'etherType': 'integer',
		'macSource': 'string',
		'macDest': 'string',
		'ipSource': 'string',
		'ipDest': 'string',
		'ipTos': 'integer',
		'ipProtocol': 'integer',
		'ipSourcePort': 'integer',
		'ipDestPort': 'integer',
		'flags': 'integer',
		'invFlags': 'integer',
		'action': 'string'
	},
	'controller/network/*/ipAssignmentPool': {
		'network': 'string',
		'netmaskBits': 'integer'
	},
	'controller/network/*/member': {
		'authorized': 'boolean',
		'activeBridge': 'boolean',
		'ipAssignments': [ 'string' ]
	},
	'controller/network/*': {
		'name': 'string',
		'private': 'boolean',
		'enableBroadcast': 'boolean',
		'allowPassiveBridging': 'boolean',
		'v4AssignMode': 'string',
		'v6AssignMode': 'string',
		'multicastLimit': 'integer',
		'relays': [ this['controller/network/*/relay'] ],
		'ipAssignmentPools': [ this['controller/network/*/ipAssignmentPool'] ],
		'rules': [ this['controller/network/*/rule'] ]
	}
};

// URL must end with trailing slash e.g. http://127.0.0.1:9993/
function ZT1ApiClient(url,authToken)
{
	this.url = url;
	this.authToken = authToken;
}

// Simple JSON URI getter, for internal use.
ZT1ApiClient.prototype._jsonGet = function(getPath,callback)
{
	request({
		url: this.url + getPath,
		method: 'GET',
		headers: {
			'X-ZT1-Auth': this.authToken
		}
	},function(error,response,body) {
		if (error)
			return callback(error,null);
		if (response.statusCode !== 200)
			return callback(new Error('server responded with error: '+response.statusCode),null);
		return callback(null,(typeof body === 'string') ? JSON.parse(body) : null);
	});
};

// Generate new ZeroTier identity -- mostly for testing
ZT1ApiClient.prototype.newIdentity = function(callback)
{
	request({
		url: this.url + 'newIdentity',
		method: 'GET',
		json: false,
		headers: {
			'X-ZT1-Auth': this.authToken
		}
	},function(error,response,body) {
		if (error)
			return callback(error,null);
		if (response.statusCode === 200)
			return callback(null,body);
		return callback(new Error('server responded with error: '+response.statusCode),'');
	});
}

// Get node status -- returns a combination of regular status and (if present) controller info
ZT1ApiClient.prototype.status = function(callback)
{
	request({
		url: this.url + 'controller',
		method: 'GET',
		headers: {
			'X-ZT1-Auth': this.authToken
		}
	},function(error,response,body) {
		if (error)
			return callback(error,null);
		var controllerStatus = {};
		if ((typeof body === 'string')&&(response.statusCode === 200))
			controllerStatus = JSON.parse(body);
		request({
			url: this.url + 'status',
			method: 'GET',
			headers: {
				'X-ZT1-Auth': this.authToken
			}
		},function(error,response,body) {
			if (error)
				return callback(error,{});
			if (response.statusCode !== 200)
				return callback(new Error('server responded with '+response.statusCode),{});
			var nodeStatus = JSON.parse(body);
			for(var k in controllerStatus)
				nodeStatus[k] = controllerStatus[k];
			return callback(null,nodeStatus);
		}.bind(this));
	}.bind(this));
};

ZT1ApiClient.prototype.getNetworks = function(callback)
{
	this._jsonGet('network',callback);
};

ZT1ApiClient.prototype.getPeers = function(callback)
{
	this._jsonGet('peer',callback);
};

ZT1ApiClient.prototype.listControllerNetworks = function(callback)
{
	this._jsonGet('controller/network',callback);
};

ZT1ApiClient.prototype.getControllerNetwork = function(nwid,callback)
{
	this._jsonGet('controller/network/' + nwid,callback);
};

// If NWID is the special ##########______ format, a new NWID will
// be generated server side and filled in in returned object.
ZT1ApiClient.prototype.saveControllerNetwork = function(network,callback)
{
	request({
		url: this.url + 'controller/network/' + n.nwid,
		method: 'POST',
		json: true,
		body: constrainTypes(network,REQUEST_TYPE_MAPS['controller/network/*']),
		headers: {
			'X-ZT1-Auth': this.authToken
		}
	},function(err,response,body) {
		if (err)
			return callback(err,null);
		if (response.statusCode !== 200)
			return callback(new Error('server responded with error: '+response.statusCode),null);
		return callback(null,(typeof body === 'string') ? JSON.parse(body) : body);
	});
};

ZT1ApiClient.prototype.deleteControllerNetwork = function(nwid,callback) {
	request({
		url: this.url + 'controller/network/'+ nwid,
		method: 'DELETE',
		headers: {
			'X-ZT1-Auth': this.authToken
		}
	},function(err,response,body) {
		if (err)
			return callback(err);
		else if (response.statusCode === 200)
			return callback(null);
		else return callback(new Error('server responded with error: '+response.statusCode));
	});
};

ZT1ApiClient.prototype.getControllerNetworkMember = function(nwid,address,callback) {
	this._jsonGet('controller/network/' + nwid + '/member/' + address,callback);
};

ZT1ApiClient.prototype.saveControllerNetworkMember = function(nwid,member,callback) {
	var m = constrainTypes(member,REQUEST_TYPE_MAPS['controller/network/*/member']);
	m.nwid = nwid;
	request({
		url: this.url + 'controller/network' + nwid + '/member/' + member.address,
		method: 'POST',
		json: true,
		body: m,
		headers: {
			'X-ZT1-Auth': this.authToken
		}
	},function(err,response,body) {
		if (err)
			return callback(err,null);
		if (response.statusCode !== 200)
			return callback(new Error('server responded with error: '+response.statusCode),null);
		return callback(null,(typeof body === 'string') ? JSON.parse(body) : body);
	});
};

ZT1ApiClient.prototype.deleteControllerNetworkMember = function(nwid,address,callback) {
	request({
		url: this.url + 'controller/network/' + nwid + '/member/' + address,
		method: 'DELETE',
		headers: {
			'X-ZT1-Auth': this.authToken
		}
	},function(err,response,body) {
		if (err)
			return callback(err);
		else if (response.statusCode === 200)
			return callback(null);
		else return callback(new Error('server responded with error: '+response.statusCode));
	});
};

exports.ZT1ApiClient = ZT1ApiClient;
