var http = require('http');
var server = http.createServer(function (request, response) {
  response.writeHead(200, {"Content-Type": "text/plain"});
  response.end("\n\nWelcome to the machine!\n\n");
});
server.listen(80);
console.log("Server running!");
