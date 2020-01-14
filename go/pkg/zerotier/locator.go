package zerotier

type Locator struct {
	Timestamp int64
	Endpoints []InetAddress
	Bytes     []byte
}
