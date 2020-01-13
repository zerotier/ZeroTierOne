package main

import (
	"github.com/zserge/webview"
)

func main() {
	w := webview.New(true)
	w.Navigate("https://github.com")
	w.SetTitle("Hello")
	w.Dispatch(func() {
		println("Hello dispatch")
	})
	w.Run()
}
