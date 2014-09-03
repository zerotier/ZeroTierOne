This folder contains the source files to compile the signed network root topology dictionary. Users outside ZeroTier won't find this useful except for testing, since the root topology must be signed by the root topology authority (public identity in root-topology-authority.public) to be considered valid.

Keys in the root topology dictionary are:

 * **supernodes**: contains another Dictionary mapping supernode address to supernode definition
   * **##########**: supernode address, contains supernode definition
     * **id**: supernode identity (public) in string-serialized format
     * **udp**: comma-delimited list of ip/port UDP addresses of node
     * **tcp**: comma-delimited list of ip/port TCP addresses of node
     * **desc**: human-readable description (optional)
     * **dns**: DNS name (optional, not currently used for anything)
 * **noupdate**: if the value of this is '1', do not auto-update from ZeroTier's servers

ZT_DEFAULT_ROOT_TOPOLOGY.c contains the current default value, and this URL is periodically checked for updates:

http://download.zerotier.com/net/topology/ROOT

Obviously nothing prevents OSS users from replacing this topology with their own, changing the hard coded topology signing identity and update URL in Defaults, and signing their own dictionary. But doing so would yield a network that would have a tough(ish) time talking to the main one. Since the main network is a free service, why bother? (Except for building testnets, which ZeroTier already does for internal testing.)
