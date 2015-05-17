'use strict'

var request = require('request');

function ZT1Client(url,authToken)
{
	this.url = url;
	this.authToken = authToken;
}

ZT1Client.prototype._jsonGet = function(getPath,callback)
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

ZT1Client.prototype.status = function(callback)
{
	request({
		url: this.url + 'controller',
		method: 'GET',
		headers: {
			'X-ZT1-Auth': this.authToken
		}
	},function(error,response,body) {
		if (error)
			return callback(error,{});
		var controllerStatus = {};
		if (typeof body === 'string')
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

ZT1Client.prototype.getNetworks = function(callback)
{
	this._jsonGet('network',callback);
};

ZT1Client.prototype.getPeers = function(callback)
{
	this._jsonGet('peer',callback);
};

ZT1Client.prototype.listControllerNetworks = function(callback)
{
	this._jsonGet('controller/network',callback);
};

ZT1Client.prototype.getControllerNetwork = function(nwid,callback)
{
	this._jsonGet('controller/network/' + nwid,callback);
};

ZT1Client.prototype.saveControllerNetwork = function(network,callback)
{
	if ((typeof network.nwid !== 'string')||(network.nwid.length !== 16))
		return callback(new Error('Missing required field: nwid'),null);

	// The ZT1 service is type variation intolerant, so recreate our submission with the correct types
	var n = {
		nwid: network.nwid
	};
	if (network.name)
		n.name = network.name.toString();
	if ('private' in network)
		n.private = (network.private) ? true : false;
	if ('enableBroadcast' in network)
		n.enableBroadcast = (network.enableBroadcast) ? true : false;
	if ('allowPassiveBridging' in network)
		n.allowPassiveBridging = (network.allowPassiveBridging) ? true : false;
	if ('v4AssignMode' in network) {
		if (network.v4AssignMode)
			n.v4AssignMode = network.v4AssignMode.toString();
		else n.v4AssignMode = 'none';
	}
	if ('v6AssignMode' in network) {
		if (network.v6AssignMode)
			n.v6AssignMode = network.v6AssignMode.toString();
		else n.v4AssignMode = 'none';
	}
	if ('multicastLimit' in network) {
		if (typeof network.multicastLimit === 'number')
			n.multicastLimit = network.multicastLimit;
		else n.multicastLimit = parseInt(network.multicastLimit.toString());
	}
	if (Array.isArray(network.relays))
		n.relays = network.relays;
	if (Array.isArray(network.ipAssignmentPools))
		n.ipAssignmentPools = network.ipAssignmentPools;
	if (Array.isArray(network.rules))
		n.rules = network.rules;

	request({
		url: this.url + 'controller/network/' + n.nwid,
		method: 'POST',
		json: true,
		body: n,
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

exports.ZT1Client = ZT1Client;
