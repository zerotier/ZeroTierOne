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
			return callback(error,{});
		if (response.statusCode !== 200)
			return callback(new Error('server responded with '+response.statusCode),{});
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

ZT1Client.prototype.networks = function(callback)
{
	this._jsonGet('network',callback);
};

ZT1Client.prototype.controllerNetworks = function(callback)
{
	this._jsonGet('controller/network',callback);
};

exports.ZT1Client = ZT1Client;
