//
// Pipe the output of server.js into this to convert raw test results into bracketed statistics
// suitable for graphing.
//

// Number of requests per statistical bracket
var BRACKET_SIZE = 1000;

// Number of bytes expected from each test
var EXPECTED_BYTES = 5000;

var readline = require('readline');
var rl = readline.createInterface({
	input: process.stdin,
	output: process.stdout,
	terminal: false
});

var count = 0.0;
var overallCount = 0.0;
var totalFailures = 0.0;
var totalOverallFailures = 0.0;
var totalMs = 0;
var totalData = 0;
var devices = {};

rl.on('line',function(line) {
	line = line.trim();
	var ls = line.split(',');
	if (ls.length == 7) {
		var ts = parseInt(ls[0]);
		var fromId = ls[1];
		var toId = ls[2];
		var ms = parseFloat(ls[3]);
		var bytes = parseInt(ls[4]);
		var timedOut = (ls[5] == 'true') ? true : false;
		var errMsg = ls[6];

		count += 1.0;
		overallCount += 1.0;
		if ((bytes !== EXPECTED_BYTES)||(timedOut)) {
			totalFailures += 1.0;
			totalOverallFailures += 1.0;
		}
		totalMs += ms;
		totalData += bytes;

		devices[fromId] = true;
		devices[toId] = true;

		if (count >= BRACKET_SIZE) {
			console.log(count.toString()+','+overallCount.toString()+','+(totalMs / count)+','+(totalFailures / count)+','+(totalOverallFailures / overallCount)+','+totalData+','+Object.keys(devices).length);

			count = 0.0;
			totalFailures = 0.0;
			totalMs = 0;
			totalData = 0;
		}
	} // else ignore junk
});
