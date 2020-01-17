package zerotier

// Root nodes are long-lived nodes at stable physical addresses that can help locate other nodes.
type Root struct {
	// Identity is this root's address and public key(s).
	Identity Identity `json:"identity"`

	// Locator describes the endpoints where this root may be found.
	Locator  Locator `json:"locator,omitempty"`

	// Bootstrap is an array of IP/port locations where this root might be found if a locator is not known.
	Bootstrap []InetAddress `json:"bootstrap,omitempty"`
}
