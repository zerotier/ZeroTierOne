var http = require('http');
var server = http.createServer(function (request, response) {
  response.writeHead(200, {"Content-Type": "text/plain"});
  response.end("Welcome to the machine!\n");
});
server.listen(8080);
console.log("Server running!");
