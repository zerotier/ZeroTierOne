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
	secrand "crypto/rand"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net"
	"net/http"
	"path"
	"strings"
	"time"

	acl "github.com/hectane/go-acl"
)

type apiStatus struct {
	Address                 Address
	Clock                   int64
	Config                  LocalConfig
	Online                  bool
	Identity                *Identity
	InterfaceAddresses      []net.IP
	MappedExternalAddresses []*InetAddress
	Version                 string
	VersionMajor            int
	VersionMinor            int
	VersionRevision         int
	VersionBuild            int
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

func apiCheckAuth(out http.ResponseWriter, req *http.Request, token string) bool {
	ah := req.Header.Get("X-ZT1-Auth")
	if len(ah) > 0 && strings.TrimSpace(ah) == token {
		return true
	}
	ah = req.Header.Get("Authorization")
	if len(ah) > 0 && strings.TrimSpace(ah) == ("bearer "+token) {
		return true
	}
	apiSendObj(out, req, http.StatusUnauthorized, nil)
	return false
}

// createAPIServer creates and starts an HTTP server for a given node
func createAPIServer(basePath string, node *Node) (*http.Server, error) {
	// Read authorization token, automatically generating one if it's missing
	var authToken string
	authTokenFile := path.Join(basePath, "authtoken.secret")
	authTokenB, err := ioutil.ReadFile(authTokenFile)
	if err != nil {
		var atb [20]byte
		_, err = secrand.Read(atb[:])
		if err != nil {
			return nil, err
		}
		for i := 0; i < 20; i++ {
			atb[i] = byte("abcdefghijklmnopqrstuvwxyz0123456789"[atb[i]%36])
		}
		err = ioutil.WriteFile(authTokenFile, atb[:], 0600)
		if err != nil {
			return nil, err
		}
		acl.Chmod(authTokenFile, 0600)
		authToken = string(atb[:])
	} else {
		authToken = strings.TrimSpace(string(authTokenB))
	}

	smux := http.NewServeMux()

	smux.HandleFunc("/status", func(out http.ResponseWriter, req *http.Request) {
		if !apiCheckAuth(out, req, authToken) {
			return
		}
		apiSetStandardHeaders(out)
		if req.Method == http.MethodGet || req.Method == http.MethodHead {
			apiSendObj(out, req, http.StatusOK, &apiStatus{
				Address:                 node.Address(),
				Clock:                   TimeMs(),
				Config:                  node.LocalConfig(),
				Online:                  node.Online(),
				Identity:                node.Identity(),
				InterfaceAddresses:      node.InterfaceAddresses(),
				MappedExternalAddresses: nil,
				Version:                 fmt.Sprintf("%d.%d.%d", CoreVersionMajor, CoreVersionMinor, CoreVersionRevision),
				VersionMajor:            CoreVersionMajor,
				VersionMinor:            CoreVersionMinor,
				VersionRevision:         CoreVersionRevision,
				VersionBuild:            CoreVersionBuild,
			})
		} else {
			out.Header().Set("Allow", "GET, HEAD")
			apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/config", func(out http.ResponseWriter, req *http.Request) {
		if !apiCheckAuth(out, req, authToken) {
			return
		}
		apiSetStandardHeaders(out)
		if req.Method == http.MethodPost || req.Method == http.MethodPut {
			var c LocalConfig
			if apiReadObj(out, req, &c) == nil {
				apiSendObj(out, req, http.StatusOK, node.LocalConfig())
			}
		} else if req.Method == http.MethodGet || req.Method == http.MethodHead {
			apiSendObj(out, req, http.StatusOK, node.LocalConfig())
		} else {
			out.Header().Set("Allow", "GET, HEAD, PUT, POST")
			apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/peer/", func(out http.ResponseWriter, req *http.Request) {
		if !apiCheckAuth(out, req, authToken) {
			return
		}
		apiSetStandardHeaders(out)

		var queriedID Address
		if len(req.URL.Path) > 6 {
			var err error
			queriedID, err = NewAddressFromString(req.URL.Path[6:])
			if err != nil {
				apiSendObj(out, req, http.StatusNotFound, nil)
				return
			}
		}

		if req.Method == http.MethodGet || req.Method == http.MethodHead {
			peers := node.Peers()
			if queriedID != 0 {
				p2 := make([]*Peer, 0, len(peers))
				for _, p := range peers {
					if p.Address == queriedID {
						p2 = append(p2, p)
					}
				}
				apiSendObj(out, req, http.StatusOK, p2)
			} else {
				apiSendObj(out, req, http.StatusOK, peers)
			}
		} else {
			out.Header().Set("Allow", "GET, HEAD")
			apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/network/", func(out http.ResponseWriter, req *http.Request) {
		if !apiCheckAuth(out, req, authToken) {
			return
		}
		apiSetStandardHeaders(out)

		var queriedID NetworkID
		if len(req.URL.Path) > 9 {
			var err error
			queriedID, err = NewNetworkIDFromString(req.URL.Path[9:])
			if err != nil {
				apiSendObj(out, req, http.StatusNotFound, nil)
				return
			}
		}

		if req.Method == http.MethodPost || req.Method == http.MethodPut {
			if queriedID == 0 {
				apiSendObj(out, req, http.StatusBadRequest, nil)
			}
		} else if req.Method == http.MethodGet || req.Method == http.MethodHead {
			if queriedID == 0 { // no queried ID lists all networks
				networks := node.Networks()
				apiSendObj(out, req, http.StatusOK, networks)
			} else {
			}
		} else {
			out.Header().Set("Allow", "GET, HEAD, PUT, POST")
			apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/root/", func(out http.ResponseWriter, req *http.Request) {
		if !apiCheckAuth(out, req, authToken) {
			return
		}
		apiSetStandardHeaders(out)

		var queriedID Address
		if len(req.URL.Path) > 6 {
			var err error
			queriedID, err = NewAddressFromString(req.URL.Path[6:])
			if err != nil {
				apiSendObj(out, req, http.StatusNotFound, nil)
				return
			}
		}

		if req.Method == http.MethodPost || req.Method == http.MethodPut {
			if queriedID == 0 {
				apiSendObj(out, req, http.StatusBadRequest, nil)
			}
		} else if req.Method == http.MethodGet || req.Method == http.MethodHead {
			roots := node.Roots()
			apiSendObj(out, req, http.StatusOK, roots)
		} else {
			out.Header().Set("Allow", "GET, HEAD, PUT, POST")
			apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	listener, err := createNamedSocketListener(basePath, "apisocket")
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
	go func() {
		httpServer.Serve(listener)
		listener.Close()
	}()

	return httpServer, nil
}
