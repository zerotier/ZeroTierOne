package main

import (
	"os"
	"runtime"
	"runtime/debug"
)

func main() {
	runtime.GOMAXPROCS(1)
	debug.SetGCPercent(15)

	if !TestCertificate() {
		os.Exit(1)
	}
}
