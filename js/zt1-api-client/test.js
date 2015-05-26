var ZT1ApiClient = require('./index.js').ZT1ApiClient;

var zt1 = new ZT1ApiClient('http://127.0.0.1:9993/','5d6181b71fae2684f9cc64ed');

zt1.status(function(err,status) {
	if (err)
		console.log(err);
	else console.log(status);

	zt1.getNetworks(function(err,networks) {
		if (err)
			console.log(err);
		else console.log(networks);

		zt1.getPeers(function(err,peers) {
			if (err)
				console.log(err);
			else console.log(peers);

			if (status.controller) {
				zt1.saveControllerNetwork({
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
