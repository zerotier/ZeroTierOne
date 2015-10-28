#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin

/zerotier-one -d
exec node --harmony /agent.js
