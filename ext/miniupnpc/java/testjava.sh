#!/bin/bash

JAVA=java
JAVAC=javac
CP=$(for i in *.jar; do echo -n $i:; done).

$JAVAC -cp $CP JavaBridgeTest.java || exit 1
$JAVA -cp $CP JavaBridgeTest 12345 UDP || exit 1
