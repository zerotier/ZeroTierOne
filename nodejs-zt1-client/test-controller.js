var ZT1Client = require('./index.js').ZT1Client;

var zt1c = new ZT1Client('http://127.0.0.1:9993/','5d6181b71fae2684f9cc64ed');

zt1c.status(function(err,status) {
	if (err)
		console.log(err);
	console.log(status);
	zt1c.networks(function(err,networks) {
		if (err)
			console.log(err);
		console.log(networks);
	});
});
