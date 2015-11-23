#!/bin/sh
# $Id: testminiwget.sh,v 1.13 2015/09/03 17:57:44 nanard Exp $
# project miniupnp : http://miniupnp.free.fr/
# (c) 2011-2015 Thomas Bernard
#
# test program for miniwget.c
# is usually invoked by "make check"
#
# This test program :
#  1 - launches a local HTTP server (minihttptestserver)
#  2 - uses testminiwget to retreive data from this server
#  3 - compares served and received data
#  4 - kills the local HTTP server and exits
#
# The script was tested and works with ksh, bash
# it should now also run with dash

TMPD=`mktemp -d -t miniwgetXXXXXXXXXX`
HTTPSERVEROUT="${TMPD}/httpserverout"
EXPECTEDFILE="${TMPD}/expectedfile"
DOWNLOADEDFILE="${TMPD}/downloadedfile"
PORT=
RET=0

case "$HAVE_IPV6" in
    n|no|0)
        ADDR=localhost
        SERVERARGS=""
        ;;
    *)
        ADDR="[::1]"
        SERVERARGS="-6"
        ;;

esac

#make minihttptestserver
#make testminiwget

# launching the test HTTP server
./minihttptestserver $SERVERARGS -e $EXPECTEDFILE > $HTTPSERVEROUT &
SERVERPID=$!
while [ -z "$PORT" ]; do
	sleep 1
	PORT=`cat $HTTPSERVEROUT | sed 's/Listening on port \([0-9]*\)/\1/' `
done
echo "Test HTTP server is listening on $PORT"

URL1="http://$ADDR:$PORT/index.html"
URL2="http://$ADDR:$PORT/chunked"
URL3="http://$ADDR:$PORT/addcrap"

echo "standard test ..."
./testminiwget $URL1 "${DOWNLOADEDFILE}.1"
if cmp $EXPECTEDFILE "${DOWNLOADEDFILE}.1" ; then
	echo "ok"
else
	echo "standard test FAILED"
	RET=1
fi

echo "chunked transfert encoding test ..."
./testminiwget $URL2 "${DOWNLOADEDFILE}.2"
if cmp $EXPECTEDFILE "${DOWNLOADEDFILE}.2" ; then
	echo "ok"
else
	echo "chunked transfert encoding test FAILED"
	RET=1
fi

echo "response too long test ..."
./testminiwget $URL3 "${DOWNLOADEDFILE}.3"
if cmp $EXPECTEDFILE "${DOWNLOADEDFILE}.3" ; then
	echo "ok"
else
	echo "response too long test FAILED"
	RET=1
fi

# kill the test HTTP server
kill $SERVERPID
wait $SERVERPID

# remove temporary files (for success cases)
if [ $RET -eq 0 ]; then
	rm -f "${DOWNLOADEDFILE}.1"
	rm -f "${DOWNLOADEDFILE}.2"
	rm -f "${DOWNLOADEDFILE}.3"
	rm -f $EXPECTEDFILE $HTTPSERVEROUT
	rmdir ${TMPD}
else
	echo "at least one of the test FAILED"
	echo "directory ${TMPD} is left intact"
fi
exit $RET

