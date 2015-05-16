'use strict'

var request = require('request');

function ZT1ControllerClient(url,authToken)
{
	this.url = url;
	this.authToken = authToken;
}

ZT1ControllerClient.prototype.status = function(callback)
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
		if (response.statusCode !== 200)
			return callback(new Error('server responded with '+response.statusCode),{});
		var controllerStatus = JSON.parse(body);
		if (controllerStatus.controller === true) {
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
		} else return callback(new Error('No "controller==true" test value present.'),{});
	}.bind(this));
};

ZT1ControllerClient.prototype.listNetworks = function(callback)
{
	request({
		url: this.url + 'controller/network',
		method: 'GET',
		headers: {
			'X-ZT1-Auth': this.authToken
		}
	},function(error,response,body) {
		if (error)
			return callback(error,{});
		if (response.statusCode !== 200)
			return callback(new Error('server responded with '+response.statusCode),{});
		var r = JSON.parse(body);
		return callback(null,Array.isArray(r) ? r : []);
	});
};

exports.ZT1ControllerClient = ZT1ControllerClient;
