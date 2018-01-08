'use strict'

/* This is a utility to convert latitude/longitude into X,Y,Z coordinates as used by clustering. */

if (process.argv.length !== 4) {
  console.log('Usage: node lat_lon_to_xyz.js <latitude> <longitude');
  process.exit(1);
}

var lat = parseFloat(process.argv[2])||0.0;
var lon = parseFloat(process.argv[3])||0.0;

var latRadians = lat * 0.01745329251994; // PI / 180
var lonRadians = lon * 0.01745329251994; // PI / 180
var cosLat = Math.cos(latRadians);

console.log({
  lat: lat,
  lon: lon,
  x: Math.round((-6371.0) * cosLat * Math.cos(lonRadians)),
  y: Math.round(6371.0 * Math.sin(latRadians)),
  z: Math.round(6371.0 * cosLat * Math.sin(lonRadians))
});

process.exit(0);
