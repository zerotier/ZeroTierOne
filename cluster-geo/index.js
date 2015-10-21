//
// GeoIP lookup service
//

// GeoIP cache TTL in ms
var CACHE_TTL = (60 * 60 * 24 * 60 * 1000); // 60 days

var config = require(__dirname + '/config.js');

if (!config.maxmind) {
	console.error('FATAL: only MaxMind GeoIP2 is currently supported and is not configured in config.js');
	process.exit(1);
}
var geo = require('geoip2ws')(config.maxmind);

var cache = require('levelup')(__dirname + '/cache.leveldb');

function lookup(ip,callback)
{
	cache.get(ip,function(err,cachedEntryJson) {
		if ((!err)&&(cachedEntryJson)) {
			try {
				var cachedEntry = JSON.parse(cachedEntryJson.toString());
				if (cachedEntry) {
					var ts = cachedEntry.ts;
					var r = cachedEntry.r;
					if ((ts)&&(r)) {
						if ((Date.now() - ts) < CACHE_TTL) {
							r._cached = true;
							return callback(null,r);
						}
					}
				}
			} catch (e) {}
		}

		geo(ip,function(err,result) {
			if (err)
				return callback(err,null);
			if ((!result)||(!result.location))
				return callback(new Error('null result'),null);

			cache.put(ip,JSON.stringify({
				ts: Date.now(),
				r: result
			}),function(err) {
				if (err)
					console.error('Error saving to cache: '+err);
				return callback(null,result);
			});
		});
	});
};

var linebuf = '';
process.stdin.on('readable',function() {
	var chunk;
	while (null !== (chunk = process.stdin.read())) {
		for(var i=0;i<chunk.length;++i) {
			var c = chunk[i];
			if ((c == 0x0d)||(c == 0x0a)) {
				if (linebuf.length > 0) {
					var ip = linebuf;
					lookup(ip,function(err,result) {
						if ((err)||(!result)||(!result.location)) {
							return process.stdout.write(ip+',0,0,0,0,0,0\n');
						} else {
							var lat = parseFloat(result.location.latitude);
							var lon = parseFloat(result.location.longitude);

							// Convert to X,Y,Z coordinates from Earth's origin, Earth-as-sphere approximation.
							var latRadians = lat * 0.01745329251994; // PI / 180
							var lonRadians = lon * 0.01745329251994; // PI / 180
							var cosLat = Math.cos(latRadians);
							var x = Math.round((-6371.0) * cosLat * Math.cos(lonRadians)); // 6371 == Earth's approximate radius in kilometers
							var y = Math.round(6371.0 * Math.sin(latRadians));
							var z = Math.round(6371.0 * cosLat * Math.sin(lonRadians));

							return process.stdout.write(ip+',1,'+lat+','+lon+','+x+','+y+','+z+'\n');
						}
					});
				}
				linebuf = '';
			} else {
				linebuf += String.fromCharCode(c);
			}
		}
	}
});

process.stdin.on('end',function() {
	cache.close();
	process.exit(0);
});
