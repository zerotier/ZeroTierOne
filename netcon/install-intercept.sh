#!/bin/bash
# This script is only needed for debugging purposes

cp libzerotierintercept.so /lib/libzerotierintercept.so
ln -sf /lib/libzerotierintercept.so /lib/libzerotierintercept
/usr/bin/install -c zerotier-intercept /usr/bin

# rm -r /lib/libzerotierintercept.so
# rm -r /lib/libzerotierintercept
# rm -r /usr/bin/zerotier-intercept
