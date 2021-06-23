#!/bin/sh

grepzt() {
  [ ! -n "$(cat /var/lib/zerotier-one/zerotier-one.pid)" -a -d "/proc/$(cat /var/lib/zerotier-one/zerotier-one.pid)" ]
  return $?
}

mkztfile() {
  file=$1
  mode=$2
  content=$3

  mkdir -p /var/lib/zerotier-one
  echo "$content" > "/var/lib/zerotier-one/$file"
  chmod "$mode" "/var/lib/zerotier-one/$file"
}

if [ "x$ZEROTIER_API_SECRET" != "x" ]
then
  mkztfile authtoken.secret 0600 "$ZEROTIER_API_SECRET"
fi

if [ "x$ZEROTIER_IDENTITY_PUBLIC" != "x" ]
then
  mkztfile identity.public 0644 "$ZEROTIER_IDENTITY_PUBLIC"
fi

if [ "x$ZEROTIER_IDENTITY_SECRET" != "x" ]
then
  mkztfile identity.secret 0600 "$ZEROTIER_IDENTITY_SECRET"
fi

mkztfile zerotier-one.port 0600 "9993"

killzerotier() {
  echo "Killing zerotier"
  kill $(cat /var/lib/zerotier-one/zerotier-one.pid)  
  exit 0
}

trap killzerotier INT TERM

echo "starting zerotier"
nohup /usr/sbin/zerotier-one &

while ! grepzt
do
  echo "zerotier hasn't started, waiting a second"
  sleep 1
done

echo "joining networks: $@"

for i in "$@"
do
  echo "joining $i"

  while ! zerotier-cli join "$i"
  do 
    echo "joining $i failed; trying again in 1s"
    sleep 1
  done
done

sleep infinity
