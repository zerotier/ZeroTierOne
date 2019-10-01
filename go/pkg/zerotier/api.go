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
	"bytes"
	secrand "crypto/rand"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net"
	"net/http"
	"path"
	"strings"
	"time"

	"github.com/hectane/go-acl"
)

// APISocketName is the default socket name for accessing the API
const APISocketName = "apisocket"

// APIGet makes a query to the API via a Unix domain or windows pipe socket
func APIGet(basePath, socketName, authToken, queryPath string, obj interface{}) (int, error) {
	client, err := createNamedSocketHTTPClient(basePath, socketName)
	if err != nil {
		return http.StatusTeapot, err
	}
	req, err := http.NewRequest("GET", "http://socket"+queryPath, nil)
	if err != nil {
		return http.StatusTeapot, err
	}
	req.Header.Add("Authorization", "bearer "+authToken)
	resp, err := client.Do(req)
	if err != nil {
		return http.StatusTeapot, err
	}
	err = json.NewDecoder(resp.Body).Decode(obj)
	return resp.StatusCode, err
}

// APIPost posts a JSON object to the API via a Unix domain or windows pipe socket and reads a response
func APIPost(basePath, socketName, authToken, queryPath string, post, result interface{}) (int, error) {
	client, err := createNamedSocketHTTPClient(basePath, socketName)
	if err != nil {
		return http.StatusTeapot, err
	}
	var data []byte
	if post != nil {
		data, err = json.Marshal(post)
		if err != nil {
			return http.StatusTeapot, err
		}
	} else {
		data = []byte("null")
	}
	req, err := http.NewRequest("POST", "http://socket"+queryPath, bytes.NewReader(data))
	if err != nil {
		return http.StatusTeapot, err
	}
	req.Header.Add("Authorization", "bearer "+authToken)
	resp, err := client.Do(req)
	if err != nil {
		return http.StatusTeapot, err
	}
	if result != nil {
		err = json.NewDecoder(resp.Body).Decode(result)
		return resp.StatusCode, err
	}
	return resp.StatusCode, nil
}

// APIStatus is the object returned by API status inquiries
type APIStatus struct {
	Address                 Address
	Clock                   int64
	Config                  LocalConfig
	Online                  bool
	PeerCount               int
	PathCount               int
	Identity                *Identity
	InterfaceAddresses      []net.IP
	MappedExternalAddresses []*InetAddress
	Version                 string
	VersionMajor            int
	VersionMinor            int
	VersionRevision         int
	VersionBuild            int
}

// APINetwork is the object returned by API network inquiries
type APINetwork struct {
	ID                     NetworkID
	Config                 *NetworkConfig
	Settings               *NetworkLocalSettings
	MulticastSubscriptions []*MulticastGroup
	TapDeviceType          string
	TapDeviceName          string
	TapDeviceEnabled       bool
}

func apiNetworkFromNetwork(n *Network) *APINetwork {
	var nn APINetwork
	nn.ID = n.ID()
	c := n.Config()
	nn.Config = &c
	ls := n.LocalSettings()
	nn.Settings = &ls
	nn.MulticastSubscriptions = n.MulticastSubscriptions()
	nn.TapDeviceType = n.Tap().Type()
	nn.TapDeviceName = n.Tap().DeviceName()
	nn.TapDeviceEnabled = n.Tap().Enabled()
	return &nn
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
		_ = apiSendObj(out, req, http.StatusBadRequest, nil)
	}
	return
}

func apiCheckAuth(out http.ResponseWriter, req *http.Request, token string) bool {
	ah := req.Header.Get("Authorization")
	if len(ah) > 0 && strings.TrimSpace(ah) == ("bearer "+token) {
		return true
	}
	ah = req.Header.Get("X-ZT1-Auth")
	if len(ah) > 0 && strings.TrimSpace(ah) == token {
		return true
	}
	_ = apiSendObj(out, req, http.StatusUnauthorized, nil)
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
			atb[i] = "abcdefghijklmnopqrstuvwxyz0123456789"[atb[i]%36]
		}
		err = ioutil.WriteFile(authTokenFile, atb[:], 0600)
		if err != nil {
			return nil, err
		}
		_ = acl.Chmod(authTokenFile, 0600)
		authToken = string(atb[:])
	} else {
		authToken = strings.TrimSpace(string(authTokenB))
	}

	smux := http.NewServeMux()

	smux.HandleFunc("/status", func(out http.ResponseWriter, req *http.Request) {
		defer func() {
			e := recover()
			if e != nil {
				_ = apiSendObj(out, req, http.StatusInternalServerError, nil)
			}
		}()

		if !apiCheckAuth(out, req, authToken) {
			return
		}

		apiSetStandardHeaders(out)
		if req.Method == http.MethodGet || req.Method == http.MethodHead {
			pathCount := 0
			peers := node.Peers()
			for _, p := range peers {
				pathCount += len(p.Paths)
			}
			_ = apiSendObj(out, req, http.StatusOK, &APIStatus{
				Address:                 node.Address(),
				Clock:                   TimeMs(),
				Config:                  node.LocalConfig(),
				Online:                  node.Online(),
				PeerCount:               len(peers),
				PathCount:               pathCount,
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
			_ = apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/config", func(out http.ResponseWriter, req *http.Request) {
		defer func() {
			e := recover()
			if e != nil {
				_ = apiSendObj(out, req, http.StatusInternalServerError, nil)
			}
		}()

		if !apiCheckAuth(out, req, authToken) {
			return
		}

		apiSetStandardHeaders(out)
		if req.Method == http.MethodPost || req.Method == http.MethodPut {
			var c LocalConfig
			if apiReadObj(out, req, &c) == nil {
				_, err := node.SetLocalConfig(&c)
				if err != nil {
					_ = apiSendObj(out, req, http.StatusBadRequest, nil)
				} else {
					_ = apiSendObj(out, req, http.StatusOK, node.LocalConfig())
				}
			}
		} else if req.Method == http.MethodGet || req.Method == http.MethodHead {
			_ = apiSendObj(out, req, http.StatusOK, node.LocalConfig())
		} else {
			out.Header().Set("Allow", "GET, HEAD, PUT, POST")
			_ = apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/peer/", func(out http.ResponseWriter, req *http.Request) {
		defer func() {
			e := recover()
			if e != nil {
				_ = apiSendObj(out, req, http.StatusInternalServerError, nil)
			}
		}()

		if !apiCheckAuth(out, req, authToken) {
			return
		}

		apiSetStandardHeaders(out)

		var queriedID Address
		if len(req.URL.Path) > 6 {
			var err error
			queriedID, err = NewAddressFromString(req.URL.Path[6:])
			if err != nil {
				_ = apiSendObj(out, req, http.StatusNotFound, nil)
				return
			}
		}

		if req.Method == http.MethodGet || req.Method == http.MethodHead {
			peers := node.Peers()
			if queriedID != 0 {
				for _, p := range peers {
					if p.Address == queriedID {
						_ = apiSendObj(out, req, http.StatusOK, p)
						return
					}
				}
				_ = apiSendObj(out, req, http.StatusNotFound, nil)
			} else {
				_ = apiSendObj(out, req, http.StatusOK, peers)
			}
		} else {
			out.Header().Set("Allow", "GET, HEAD")
			_ = apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/network/", func(out http.ResponseWriter, req *http.Request) {
		defer func() {
			e := recover()
			if e != nil {
				_ = apiSendObj(out, req, http.StatusInternalServerError, nil)
			}
		}()

		if !apiCheckAuth(out, req, authToken) {
			return
		}

		apiSetStandardHeaders(out)

		var queriedID NetworkID
		if len(req.URL.Path) > 9 {
			var err error
			queriedID, err = NewNetworkIDFromString(req.URL.Path[9:])
			if err != nil {
				_ = apiSendObj(out, req, http.StatusNotFound, nil)
				return
			}
		}

		if req.Method == http.MethodDelete {
			if queriedID == 0 {
				_ = apiSendObj(out, req, http.StatusBadRequest, nil)
			} else {
				networks := node.Networks()
				for _, nw := range networks {
					if nw.id == queriedID {
						_ = node.Leave(queriedID)
						_ = apiSendObj(out, req, http.StatusOK, nw)
						return
					}
				}
				_ = apiSendObj(out, req, http.StatusNotFound, nil)
			}
		} else if req.Method == http.MethodPost || req.Method == http.MethodPut {
			if queriedID == 0 {
				_ = apiSendObj(out, req, http.StatusBadRequest, nil)
			} else {
				var nw APINetwork
				if apiReadObj(out, req, &nw) == nil {
					n := node.GetNetwork(nw.ID)
					if n == nil {
						n, err := node.Join(nw.ID, nw.Settings, nil)
						if err != nil {
							_ = apiSendObj(out, req, http.StatusBadRequest, nil)
						} else {
							_ = apiSendObj(out, req, http.StatusOK, apiNetworkFromNetwork(n))
						}
					} else {
						if nw.Settings != nil {
							n.SetLocalSettings(nw.Settings)
						}
						_ = apiSendObj(out, req, http.StatusOK, apiNetworkFromNetwork(n))
					}
				}
			}
		} else if req.Method == http.MethodGet || req.Method == http.MethodHead {
			networks := node.Networks()
			if queriedID == 0 { // no queried ID lists all networks
				nws := make([]*APINetwork, 0, len(networks))
				for _, nw := range networks {
					nws = append(nws, apiNetworkFromNetwork(nw))
				}
				_ = apiSendObj(out, req, http.StatusOK, nws)
			} else {
				for _, nw := range networks {
					if nw.ID() == queriedID {
						_ = apiSendObj(out, req, http.StatusOK, apiNetworkFromNetwork(nw))
						return
					}
				}
				_ = apiSendObj(out, req, http.StatusNotFound, nil)
			}
		} else {
			out.Header().Set("Allow", "GET, HEAD, PUT, POST, DELETE")
			_ = apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	smux.HandleFunc("/root/", func(out http.ResponseWriter, req *http.Request) {
		defer func() {
			e := recover()
			if e != nil {
				_ = apiSendObj(out, req, http.StatusInternalServerError, nil)
			}
		}()

		if !apiCheckAuth(out, req, authToken) {
			return
		}

		apiSetStandardHeaders(out)

		var queriedName string
		if len(req.URL.Path) > 6 {
			queriedName = req.URL.Path[6:]
		}

		if req.Method == http.MethodDelete {
			if len(queriedName) > 0 {
				roots := node.Roots()
				for _, r := range roots {
					if r.Name == queriedName {
						node.RemoveRoot(queriedName)
						_ = apiSendObj(out, req, http.StatusOK, r)
						return
					}
				}
			}
			_ = apiSendObj(out, req, http.StatusNotFound, nil)
		} else if req.Method == http.MethodPost || req.Method == http.MethodPut {
			var r Root
			if apiReadObj(out, req, &r) == nil {
				if r.Name != queriedName {
					_ = apiSendObj(out, req, http.StatusBadRequest, nil)
					return
				}
				err := node.SetRoot(r.Name, r.Locator)
				if err != nil {
					_ = apiSendObj(out, req, http.StatusBadRequest, nil)
				} else {
					roots := node.Roots()
					for _, r := range roots {
						if r.Name == queriedName {
							_ = apiSendObj(out, req, http.StatusOK, r)
							return
						}
					}
					_ = apiSendObj(out, req, http.StatusNotFound, nil)
				}
			}
		} else if req.Method == http.MethodGet || req.Method == http.MethodHead {
			roots := node.Roots()
			for _, r := range roots {
				if r.Name == queriedName {
					_ = apiSendObj(out, req, http.StatusOK, r)
					return
				}
			}
			_ = apiSendObj(out, req, http.StatusNotFound, nil)
		} else {
			out.Header().Set("Allow", "GET, HEAD, PUT, POST, DELETE")
			_ = apiSendObj(out, req, http.StatusMethodNotAllowed, nil)
		}
	})

	listener, err := createNamedSocketListener(basePath, APISocketName)
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
		err := httpServer.Serve(listener)
		if err != nil {
			node.log.Printf("ERROR: unable to start API HTTP server: %s (continuing anyway but CLI will not work!)", err.Error())
		}
		_ = listener.Close()
	}()

	return httpServer, nil
}
