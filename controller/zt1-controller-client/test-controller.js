var ZT1ControllerClient = require('./index.js').ZT1ControllerClient;

var zt1c = new ZT1ControllerClient('http://127.0.0.1:9993/','5d6181b71fae2684f9cc64ed');

zt1c.status(function(err,status) {
	if (err)
		console.log(err);
	console.log(status);
	zt1c.listNetworks(function(err,networks) {
		if (err)
			console.log(err);
		console.log(networks);
	});
});
