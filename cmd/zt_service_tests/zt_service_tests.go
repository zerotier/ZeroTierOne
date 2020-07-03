package main

import "os"

func main() {
	if !TestCertificate() {
		os.Exit(1)
	}
}

