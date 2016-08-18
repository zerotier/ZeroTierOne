./zerotier 
zerotier-cli join $(NWID).conf
zerotier-cli net-auth $(NWID) $(DEVID)