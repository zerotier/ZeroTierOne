var ZT1Client = require('./index.js').ZT1Client;

var zt1c = new ZT1Client('http://127.0.0.1:9993/','5d6181b71fae2684f9cc64ed');

zt1c.status(function(err,status) {
	if (err)
		console.log(err);
	else console.log(status);

	zt1c.getNetworks(function(err,networks) {
		if (err)
			console.log(err);
		else console.log(networks);

		zt1c.getPeers(function(err,peers) {
			if (err)
				console.log(err);
			else console.log(peers);

			if (status.controller) {
				zt1c.saveControllerNetwork({
					nwid: status.address + 'dead01',
					name: 'test network',
					private: true
				},function(err,network) {
					if (err)
						console.log(err);
					else console.log(network);
				});
			}
		});
	});
});
