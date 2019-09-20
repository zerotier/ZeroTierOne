package main

import (
	"fmt"
	"net"
)

func main() {
	ifs, err := net.Interfaces()
	if err != nil {
		fmt.Printf("Error: %s\n", err.Error())
		return
	}
	for _, i := range ifs {
		fmt.Printf("name: %s\n", i.Name)
		fmt.Printf("hwaddr: %s\n", i.HardwareAddr.String())
		fmt.Printf("index: %d\n", i.Index)
		fmt.Printf("addrs:\n")
		addrs, _ := i.Addrs()
		for _, a := range addrs {
			fmt.Printf("  %s\n", a.String())
		}
		fmt.Printf("multicast:\n")
		mc, _ := i.MulticastAddrs()
		for _, m := range mc {
			fmt.Printf("  %s\n", m.String())
		}
		fmt.Printf("\n")
	}
}
