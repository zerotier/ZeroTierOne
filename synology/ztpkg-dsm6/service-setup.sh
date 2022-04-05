
service_postinst()
{
	exit 0
}

service_postuninst()
{
	# remove all files except for identity files and network config files (for future convenience)
	find /var/lib/zerotier-one/* -type f -o -type d ! -name 'identity.*' -delete
	exit 0
}

service_postupgrade()
{
	exit 0
}

service_preinst()
{
	exit 0
}

service_preuninst()
{
	exit 0
}

service_preupgrade()
{
	exit 0
}
