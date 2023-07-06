Network Controller Microservice
======

Every ZeroTier virtual network has a *network controller* responsible for admitting members to the network, issuing certificates, and issuing default configuration information.

This is our reference controller implementation and is almost the same as the one we use to power our own hosted services at [my.zerotier.com](https://my.zerotier.com/). The only difference is the database backend used. 

Controller data is stored in JSON format under `controller.d` in the ZeroTier working directory. It can be copied, rsync'd, placed in `git`, etc. The files under `controller.d` should not be modified in place while the controller is running or data loss may result, and if they are edited directly take care not to save corrupt JSON since that can also lead to data loss when the controller is restarted. Going through the API is strongly preferred to directly modifying these files.

See the API section below for information about controlling the controller.

### Scalability and Reliability

Controllers can in theory host up to 2^24 networks and serve many millions of devices (or more), but we recommend spreading large numbers of networks across many controllers for load balancing and fault tolerance reasons. Since the controller uses the filesystem as its data store we recommend fast filesystems and fast SSD drives for heavily loaded controllers.

Since ZeroTier nodes are mobile and do not need static IPs, implementing high availability fail-over for controllers is easy. Just replicate their working directories from master to backup and have something automatically fire up the backup if the master goes down. Modern orchestration tools like Nomad and Kubernetes can be of help here.

### Dockerizing Controllers

ZeroTier network controllers can easily be run in Docker or other container systems. Since containers do not need to actually join networks, extra privilege options like "--device=/dev/net/tun --privileged" are not needed. You'll just need to map the local JSON API port of the running controller and allow it to access the Internet (over UDP/9993 at a minimum) so things can reach and query it.

### Upgrading from Older (1.1.14 or earlier) Versions

Older versions of this code used a SQLite database instead of in-filesystem JSON. A migration utility called `migrate-sqlite` is included here and *must* be used to migrate this data to the new format. If the controller is started with an old `controller.db` in its working directory it will terminate after printing an error to *stderr*. This is done to prevent "surprises" for those running DIY controllers using the old code.

The migration tool is written in nodeJS and can be used like this:

    cd migrate-sqlite
    npm install
    node migrate.js </path/to/controller.db> </path/to/controller.d>

### Network Controller API

The controller API is hosted via the same JSON API endpoint that ZeroTier One uses for local control (usually at 127.0.0.1 port 9993). All controller options are routed under the `/controller` base path.

The controller microservice itself does not implement any fine-grained access control. Access control is via the ZeroTier control interface itself and `authtoken.secret`. This can be sent as the `X-ZT1-Auth` HTTP header field or appended to the URL as `?auth=<token>`. Take care when doing the latter that request URLs are not being logged.

While networks with any valid ID can be added to the controller's database, it will only actually work to control networks whose first 10 hex digits correspond with the network controller's ZeroTier ID. See [section 2.2.1 of the ZeroTier manual](https://zerotier.com/manual.shtml#2_2_1).

The controller JSON API is *very* sensitive about types. Integers must be integers and strings strings, etc. Incorrect types may be ignored, set to default values, or set to undefined values.

Full documentation of the Controller API can be found on our [documentation site](https://docs.zerotier.com/service/v1#tag/controller)

### Prometheus Metrics

Controller specific metrics are available from the `/metrics` endpoint.

| Metric Name | Type | Description |
| --- | --- | --- |
| controller_network_count | Gauge | number of networks the controller is serving | 
| controller_member_count | Gauge | number of network members the controller is serving |
| controller_network_change_count | Counter | number of times a network configuration is changed |
| controller_member_change_count | Counter | number of times a network member configuration is changed |
| controller_member_auth_count | Counter | number of network member auths |
| controller_member_deauth_count | Counter | number of network member deauths|
