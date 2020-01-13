package zerotier

// Root is a root server with one or more permanent IPs.
type Root struct {
	Identity Identity
	DNSName string
	PhysicalAddresses []InetAddress
}
