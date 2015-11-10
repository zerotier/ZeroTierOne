// ZeroTier distributed HTTP test coordinator and result-reporting server

// ---------------------------------------------------------------------------
// Customizable parameters:

var SERVER_PORT = 18080;

// ---------------------------------------------------------------------------

var fs = require('fs');

var express = require('express');
var app = express();

app.use(function(req,res,next) {
	req.rawBody = '';
	req.on('data', function(chunk) { req.rawBody += chunk.toString(); });
	req.on('end', function() { return next(); });
});

var knownAgents = {};

app.post('/:agentId',function(req,res) {
	var agentId = req.params.agentId;
	if ((!agentId)||(agentId.length !== 32))
		return res.status(404).send('');

	if (req.rawBody) {
		var receiveTime = Date.now();
		var resultData = null;
		try {
			resultData = JSON.parse(req.rawBody);
			console.log(Date.now().toString()+','+resultData.source+','+resultData.target+','+resultData.time+','+resultData.bytes+','+resultData.timedOut+',"'+((resultData.error) ? resultData.error : '')+'"');
		} catch (e) {}
	}

	knownAgents[agentId] = true;
	var thisUpdate = [];
	var agents = Object.keys(knownAgents);
	if (agents.length < 100)
		thisUpdate = agents;
	else {
		for(var xx=0;xx<100;++xx)
			thisUpdate.push(agents[Math.floor(Math.random() * agents.length)]);
	}

	return res.status(200).send(JSON.stringify(thisUpdate));
});

var expressServer = app.listen(SERVER_PORT,function () {
	console.log('LISTENING ON '+SERVER_PORT);
	console.log('');
});
