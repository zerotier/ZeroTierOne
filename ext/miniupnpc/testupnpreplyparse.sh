#!/bin/sh

for f in testreplyparse/*.xml ; do
	bf="`dirname $f`/`basename $f .xml`"
	if ./testupnpreplyparse $f $bf.namevalue ; then
		echo "$f : passed"
	else
		echo "$f : FAILED"
		exit 1
	fi
done

exit 0

