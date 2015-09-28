#!/bin/bash

rm -rf /run/httpd/* /tmp/httpd*

exec /usr/sbin/httpd -D FOREGROUND
