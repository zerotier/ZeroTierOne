// ---------------------------------------------------------------------------
// Customizable parameters:

var SERVER_PORT = 18080;

// ---------------------------------------------------------------------------

var express = require('express');
var app = express();

app.use(function(req,res,next) {
	req.rawBody = '';
	req.on('data', function(chunk) { req.rawBody += chunk.toString(); });
	req.on('end', function() { return next(); });
});

var knownAgents = {};

app.get('/:agentId',function(req,res) {
	var agentId = req.params.agentId;
	if ((!agentId)||(agentId.length !== 32))
		return res.status(404).send('');
	knownAgents[agentId] = Date.now();
	return res.status(200).send(JSON.stringify(Object.keys(knownAgents)));
});

app.post('/:agentId',function(req,res) {
	var agentId = req.params.agentId;
	if ((!agentId)||(agentId.length !== 32))
		return res.status(404).send('');
	var resultData = null;
	try {
		resultData = JSON.parse(req.rawBody);
	} catch (e) {
		resultData = req.rawBody;
	}
	result = {
		agentId: agentId,
		result: resultData
	};
	console.log(result);
	return res.status(200).send('');
});

var expressServer = app.listen(SERVER_PORT,function () {
	console.log('LISTENING ON '+SERVER_PORT);
	console.log('');
});
