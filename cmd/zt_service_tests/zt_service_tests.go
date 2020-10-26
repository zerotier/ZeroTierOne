package main

import (
	"os"
	"runtime"
	"runtime/debug"
)

func main() {
	runtime.GOMAXPROCS(1)
	debug.SetGCPercent(10)

	if !TestCertificate() {
		os.Exit(1)
	}
	if !TestLocator() {
		os.Exit(1)
	}
}
