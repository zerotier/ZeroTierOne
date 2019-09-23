/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package zerotier

import (
	"encoding/json"
	"net"
	"net/http"
	"path"
	"time"
)

type apiStatus struct {
	Address         Address
	Clock           int64
	Config          *LocalConfig
	Online          bool
	Identity        *Identity
	Version         string
	VersionMajor    int
	VersionMinor    int
	VersionRevision int
	VersionBuild    int
}

type apiNetwork struct {
	Config                 *NetworkConfig
	Settings               *NetworkLocalSettings
	MulticastSubscriptions []*MulticastGroup
	TapDeviceType          string
	TapDeviceName          string
	TapDeviceEnabled       bool
}

func apiSetStandardHeaders(out http.ResponseWriter) {
	now := time.Now().UTC()
	h := out.Header()
	h.Set("Cache-Control", "no-cache, no-store, must-revalidate")
	h.Set("Expires", "0")
	h.Set("Pragma", "no-cache")
	h.Set("Date", now.Format(time.RFC1123))
}

func apiSendObj(out http.ResponseWriter, req *http.Request, httpStatusCode int, obj interface{}) error {
	h := out.Header()
	h.Set("Content-Type", "application/json")
	if req.Method == http.MethodHead {
		out.WriteHeader(httpStatusCode)
		return nil
	}
	var j []byte
	var err error
	if obj != nil {
		j, err = json.Marshal(obj)
		if err != nil {
			return err
		}
	}
	out.WriteHeader(httpStatusCode)
	_, err = out.Write(j)
	return err
}

func apiReadObj(out http.ResponseWriter, req *http.Request, dest interface{}) (err error) {
	err = json.NewDecoder(req.Body).Decode(&dest)
	if err != nil {
		apiSendObj(out, req, http.StatusBadRequest, nil)
	}
	return
}

// createAPIServer creates and starts an HTTP server for a given node
func createAPIServer(basePath string, node *Node) (*http.Server, error) {
	smux := http.NewServeMux()

	smux.HandleFunc("/config", func(out http.ResponseWriter, req *http.Request) {
		apiSetStandardHeaders(out)
		if req.Method == http.MethodGet || req.Method == http.MethodHead {
			apiSendObj(out, req, http.StatusOK, nil)
		} else {
			out.Header().Set("Allow", "GET, HEAD")
			apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/status", func(out http.ResponseWriter, req *http.Request) {
		apiSetStandardHeaders(out)
		if req.Method == http.MethodGet || req.Method == http.MethodHead {
			var status apiStatus
			apiSendObj(out, req, http.StatusOK, &status)
		} else {
			out.Header().Set("Allow", "GET, HEAD")
			apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/peer/", func(out http.ResponseWriter, req *http.Request) {
		apiSetStandardHeaders(out)
		if req.Method == http.MethodGet || req.Method == http.MethodHead {
			peers := node.Peers()
			apiSendObj(out, req, http.StatusOK, peers)
		} else {
			out.Header().Set("Allow", "GET, HEAD")
			apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/network/", func(out http.ResponseWriter, req *http.Request) {
		apiSetStandardHeaders(out)
		if req.Method == http.MethodGet || req.Method == http.MethodHead {
			networks := node.Networks()
			apiSendObj(out, req, http.StatusOK, networks)
		} else {
			out.Header().Set("Allow", "GET, HEAD")
			apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	unixListener, err := net.Listen("unix", path.Join(basePath, "apisocket"))
	if err != nil {
		return nil, err
	}
	httpServer := &http.Server{
		MaxHeaderBytes: 4096,
		Handler:        smux,
		IdleTimeout:    10 * time.Second,
		ReadTimeout:    10 * time.Second,
		WriteTimeout:   600 * time.Second,
	}
	httpServer.SetKeepAlivesEnabled(true)
	go httpServer.Serve(unixListener)

	return httpServer, nil
}
