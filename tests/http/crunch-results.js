//
// Pipe the output of server.js into this to convert raw test results into bracketed statistics
// suitable for graphing.
//

// Average over this interval of time
var GRAPH_INTERVAL = 60000;

// Number of bytes expected from each test
var EXPECTED_BYTES = 5000;

var readline = require('readline');
var rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  terminal: false
});

var startTS = 0;

var count = 0.0;
var totalFailures = 0;
var totalPartialFailures = 0;
var totalMs = 0;
var totalData = 0;

rl.on('line',function(line) {
  line = line.trim();
  var ls = line.split(',');
  if (ls.length == 7) {
    var ts = parseInt(ls[0]);
    var from = ls[1];
    var to = ls[2];
    var ms = parseFloat(ls[3]);
    var bytes = parseInt(ls[4]);
    var timedOut = (ls[5] == 'true') ? true : false;
    var errMsg = ls[6];

    count += 1.0;
    if ((bytes <= 0)||(timedOut))
      ++totalFailures;
    if (bytes !== EXPECTED_BYTES)
      ++totalPartialFailures;
    totalMs += ms;
    totalData += bytes;

    if (startTS === 0) {
      startTS = ts;
    } else if (((ts - startTS) >= GRAPH_INTERVAL)&&(count > 0.0)) {
      console.log(count.toString()+','+(totalMs / count)+','+totalFailures+','+totalPartialFailures+','+totalData);

      count = 0.0;
      totalFailures = 0;
      totalPartialFailures = 0;
      totalMs = 0;
    }
  } // else ignore junk
});
