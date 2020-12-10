package main

import (
	"os"
	"runtime"
	"runtime/debug"
)

const numToRun = 10000

func main() {
	runtime.GOMAXPROCS(1)
	debug.SetGCPercent(10)

	for k:=0;k<numToRun;k++ {
		if !TestCertificate(){
		os.Exit(1)
	}
		if !TestLocator(){
		os.Exit(1)
	}
	}
}
