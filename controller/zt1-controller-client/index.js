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
		if ((error)||(response.statusCode !== 200))
			return callback(error,{});
		return callback(null,JSON.parse(body));
	});
};

exports.ZT1ControllerClient = ZT1ControllerClient;
