Cluster GeoIP Service
======

In cluster mode (build with ZT\_ENABLE\_CLUSTER and install a cluster definition file), ZeroTier One can use geographic IP lookup to steer clients toward members of a cluster that are physically closer and are therefore very likely to offer lower latency and better performance. Ordinary non-clustered ZeroTier endpoints will have no use for this code.

If a cluster-mode instance detects a file in the ZeroTier home folder called *cluster-geo.exe*, it attempts to execute it. If this program runs, it receives IP addresses on STDIN and produces lines of CSV on STDOUT with the following format:

    IP,result code,latitude,longitude,x,y,z

IPv6 IPs must be sent *without* compression / zero-removal.

The first field is the IP echoed back. The second field is 0 if the result is pending and may be ready in the future or 1 if the result is ready now. If the second field is 0 the remaining fields should be 0. Otherwise the remaining fields contain the IP's latitude, longitude, and X/Y/Z coordinates.

ZeroTier's cluster route optimization code only uses the X/Y/Z values. These are computed by this cluster-geo code as the spherical coordinates of the IP address using the Earth's center as the point of origin and using an approximation of the Earth as a sphere. This doesn't yield *exact* coordinates, but it's good enough for our purposes since the goal is to route clients to the geographically closest endpoint.

To install, copy *cluster-geo.exe* and the *cluster-geo/* subfolder into the ZeroTier home. Then go into *cluster-geo/* and run *npm install* to install the project's dependencies. A recent (4.x or newer) version of NodeJS is recommended. You will also need a [MaxMind GeoIP2 Precision Services](https://www.maxmind.com/) license key. The *MaxMind GeoIP2 City* tier is required since this supplies actual coordinates. It's a commercial service but is very inexpensive and offers very good accuracy for both IPv4 and IPv6 addresses. The *cluster-geo.js* program caches results in a LevelDB database for up to 120 days to reduce GeoIP API queries.
