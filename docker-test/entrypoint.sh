#!/bin/bash

./zerotier-one -d -U -p9993

rm -rf /run/httpd/* /tmp/httpd*
exec intercept /usr/sbin/httpd -D FOREGROUND
