/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
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
	"io"
	"io/ioutil"
	"net"
	"net/http"
	"path"
	"runtime"
	"strconv"
	"strings"
	"time"

	"github.com/hectane/go-acl"
)

// APISocketName is the default socket name for accessing the API
const APISocketName = "apisocket"

// APIGet makes a query to the API via a Unix domain or windows pipe socket
func APIGet(basePath, socketName, authToken, queryPath string, obj interface{}) (int, int64, error) {
	client, err := createNamedSocketHTTPClient(basePath, socketName)
	if err != nil {
		return http.StatusTeapot, 0, err
	}
	req, err := http.NewRequest("GET", "http://socket"+queryPath, nil)
	if err != nil {
		return http.StatusTeapot, 0, err
	}
	req.Header.Add("Authorization", "bearer "+authToken)
	resp, err := client.Do(req)
	if err != nil {
		return http.StatusTeapot, 0, err
	}
	err = json.NewDecoder(resp.Body).Decode(obj)
	ts := resp.Header.Get("X-ZT-Clock")
	t := int64(0)
	if len(ts) > 0 {
		t, _ = strconv.ParseInt(ts, 10, 64)
	}
	return resp.StatusCode, t, err
}

// APIPost posts a JSON object to the API via a Unix domain or windows pipe socket and reads a response
func APIPost(basePath, socketName, authToken, queryPath string, post, result interface{}) (int, int64, error) {
	client, err := createNamedSocketHTTPClient(basePath, socketName)
	if err != nil {
		return http.StatusTeapot, 0, err
	}
	var data []byte
	if post != nil {
		data, err = json.Marshal(post)
		if err != nil {
			return http.StatusTeapot, 0, err
		}
	} else {
		data = []byte("null")
	}
	req, err := http.NewRequest("POST", "http://socket"+queryPath, bytes.NewReader(data))
	if err != nil {
		return http.StatusTeapot, 0, err
	}
	req.Header.Add("Content-Type", "application/json")
	req.Header.Add("Authorization", "bearer "+authToken)
	resp, err := client.Do(req)
	if err != nil {
		return http.StatusTeapot, 0, err
	}
	ts := resp.Header.Get("X-ZT-Clock")
	t := int64(0)
	if len(ts) > 0 {
		t, _ = strconv.ParseInt(ts, 10, 64)
	}
	if result != nil {
		err = json.NewDecoder(resp.Body).Decode(result)
		return resp.StatusCode, t, err
	}
	return resp.StatusCode, t, nil
}

// APIDelete posts DELETE to a path and fills result with the outcome (if any) if result is non-nil
func APIDelete(basePath, socketName, authToken, queryPath string, result interface{}) (int, int64, error) {
	client, err := createNamedSocketHTTPClient(basePath, socketName)
	if err != nil {
		return http.StatusTeapot, 0, err
	}
	req, err := http.NewRequest("DELETE", "http://socket"+queryPath, nil)
	if err != nil {
		return http.StatusTeapot, 0, err
	}
	req.Header.Add("Authorization", "bearer "+authToken)
	resp, err := client.Do(req)
	if err != nil {
		return http.StatusTeapot, 0, err
	}
	ts := resp.Header.Get("X-ZT-Clock")
	t := int64(0)
	if len(ts) > 0 {
		t, _ = strconv.ParseInt(ts, 10, 64)
	}
	if result != nil {
		err = json.NewDecoder(resp.Body).Decode(result)
		return resp.StatusCode, t, err
	}
	return resp.StatusCode, t, nil
}

// APIStatus is the object returned by API status inquiries
type APIStatus struct {
	Address                 Address        `json:"address"`
	Clock                   int64          `json:"clock"`
	StartupTime             int64          `json:"startupTime"`
	Config                  *LocalConfig   `json:"config"`
	Online                  bool           `json:"online"`
	PeerCount               int            `json:"peerCount"`
	PathCount               int            `json:"pathCount"`
	Identity                *Identity      `json:"identity"`
	InterfaceAddresses      []net.IP       `json:"interfaceAddresses,omitempty"`
	MappedExternalAddresses []*InetAddress `json:"mappedExternalAddresses,omitempty"`
	Version                 string         `json:"version"`
	VersionMajor            int            `json:"versionMajor"`
	VersionMinor            int            `json:"versionMinor"`
	VersionRevision         int            `json:"versionRevision"`
	VersionBuild            int            `json:"versionBuild"`
	OS                      string         `json:"os"`
	Architecture            string         `json:"architecture"`
	Concurrency             int            `json:"concurrency"`
	Runtime                 string         `json:"runtimeVersion"`
}

// APINetwork is the object returned by API network inquiries
type APINetwork struct {
	ID                     NetworkID             `json:"id"`
	Config                 NetworkConfig         `json:"config"`
	Settings               *NetworkLocalSettings `json:"settings,omitempty"`
	ControllerFingerprint  *Fingerprint          `json:"controllerFingerprint,omitempty"`
	MulticastSubscriptions []*MulticastGroup     `json:"multicastSubscriptions,omitempty"`
	PortType               string                `json:"portType"`
	PortName               string                `json:"portName"`
	PortEnabled            bool                  `json:"portEnabled"`
	PortErrorCode          int                   `json:"portErrorCode"`
	PortError              string                `json:"portError"`
}

func apiNetworkFromNetwork(n *Network) *APINetwork {
	var nn APINetwork
	nn.ID = n.ID()
	nn.Config = n.Config()
	ls := n.LocalSettings()
	nn.Settings = &ls
	nn.MulticastSubscriptions = n.MulticastSubscriptions()
	nn.PortType = n.Tap().Type()
	nn.PortName = n.Tap().DeviceName()
	nn.PortEnabled = n.Tap().Enabled()
	ec, errStr := n.Tap().Error()
	nn.PortErrorCode = ec
	nn.PortError = errStr
	return &nn
}

func apiSetStandardHeaders(out http.ResponseWriter) {
	h := out.Header()
	h.Set("Cache-Control", "no-cache, no-store, must-revalidate")
	h.Set("Expires", "0")
	h.Set("Pragma", "no-cache")
	t := time.Now().UTC()
	h.Set("Date", t.Format(time.RFC1123))
	h.Set("X-ZT-Clock", strconv.FormatInt(t.UnixNano()/int64(1000000), 10))
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
		_ = apiSendObj(out, req, http.StatusBadRequest, &APIErr{"invalid JSON: " + err.Error()})
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
	_ = apiSendObj(out, req, http.StatusUnauthorized, &APIErr{"authorization token not found or incorrect (checked X-ZT1-Auth and Authorization headers)"})
	return false
}

// createAPIServer creates and starts an HTTP server for a given node
func createAPIServer(basePath string, node *Node) (*http.Server, *http.Server, error) {
	// Read authorization token, automatically generating one if it's missing
	var authToken string
	authTokenFile := path.Join(basePath, "authtoken.secret")
	authTokenB, err := ioutil.ReadFile(authTokenFile)
	if err != nil {
		var atb [20]byte
		_, err = secrand.Read(atb[:])
		if err != nil {
			return nil, nil, err
		}
		for i := 0; i < 20; i++ {
			atb[i] = "abcdefghijklmnopqrstuvwxyz0123456789"[atb[i]%36]
		}
		err = ioutil.WriteFile(authTokenFile, atb[:], 0600)
		if err != nil {
			return nil, nil, err
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
				StartupTime:             node.startupTime,
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
				OS:                      runtime.GOOS,
				Architecture:            runtime.GOARCH,
				Concurrency:             runtime.NumCPU(),
				Runtime:                 runtime.Version(),
			})
		} else {
			out.Header().Set("Allow", "GET, HEAD")
			_ = apiSendObj(out, req, http.StatusMethodNotAllowed, &APIErr{"/status is read-only"})
		}
	})

	smux.HandleFunc("/config", func(out http.ResponseWriter, req *http.Request) {
		defer func() {
			e := recover()
			if e != nil {
				_ = apiSendObj(out, req, http.StatusInternalServerError, &APIErr{"caught unexpected error in request handler"})
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
					_ = apiSendObj(out, req, http.StatusBadRequest, &APIErr{"error applying local config: " + err.Error()})
				} else {
					lc := node.LocalConfig()
					_ = apiSendObj(out, req, http.StatusOK, &lc)
				}
			}
		} else if req.Method == http.MethodGet || req.Method == http.MethodHead {
			_ = apiSendObj(out, req, http.StatusOK, node.LocalConfig())
		} else {
			out.Header().Set("Allow", "GET, HEAD, PUT, POST")
			_ = apiSendObj(out, req, http.StatusMethodNotAllowed, &APIErr{"unsupported method: " + req.Method})
		}
	})

	smux.HandleFunc("/peer/", func(out http.ResponseWriter, req *http.Request) {
		var err error

		defer func() {
			e := recover()
			if e != nil {
				_ = apiSendObj(out, req, http.StatusInternalServerError, &APIErr{"caught unexpected error in request handler"})
			}
		}()

		if !apiCheckAuth(out, req, authToken) {
			return
		}
		apiSetStandardHeaders(out)

		if req.URL.Path == "/peer/_addroot" {
			if req.Method == http.MethodPost || req.Method == http.MethodPut {
				rsdata, err := ioutil.ReadAll(io.LimitReader(req.Body, 16384))
				if err != nil || len(rsdata) == 0 {
					_ = apiSendObj(out, req, http.StatusBadRequest, &APIErr{"read error"})
				} else {
					// TODO
					_ = apiSendObj(out, req, http.StatusOK, nil)
				}
			} else {
				out.Header().Set("Allow", "POST, PUT")
				_ = apiSendObj(out, req, http.StatusMethodNotAllowed, &APIErr{"no root spec supplied"})
			}
			return
		}

		var queriedStr string
		var queriedID Address
		var queriedFP *Fingerprint
		if len(req.URL.Path) > 6 {
			queriedStr = req.URL.Path[6:]
			if len(queriedStr) == AddressStringLength {
				queriedID, err = NewAddressFromString(queriedStr)
				if err != nil {
					_ = apiSendObj(out, req, http.StatusNotFound, &APIErr{"peer not found"})
					return
				}
			} else {
				queriedFP, err = NewFingerprintFromString(queriedStr)
				if err != nil {
					_ = apiSendObj(out, req, http.StatusNotFound, &APIErr{"peer not found"})
					return
				}
			}
		}

		var peer *Peer
		peers := node.Peers()
		if queriedFP != nil {
			for _, p := range peers {
				if p.Fingerprint.Equals(queriedFP) {
					peer = p
					break
				}
			}
		} else if queriedID != 0 {
			for _, p := range peers {
				if p.Address == queriedID {
					peer = p
					break
				}
			}
		}

		if req.Method == http.MethodPost || req.Method == http.MethodPut {
			if peer != nil {
				var posted Peer
				if apiReadObj(out, req, &posted) == nil {
					if posted.Root && !peer.Root {
						_ = apiSendObj(out, req, http.StatusBadRequest, &APIErr{"root spec must be submitted to /peer/_addroot, post to peers can only be used to clear the root flag"})
					} else if !posted.Root && peer.Root {
						peer.Root = false
						node.RemoveRoot(peer.Address)
						_ = apiSendObj(out, req, http.StatusOK, peer)
					}
				}
			} else {
				_ = apiSendObj(out, req, http.StatusNotFound, &APIErr{"peer not found"})
			}
		} else if req.Method == http.MethodGet || req.Method == http.MethodHead || req.Method == http.MethodPost || req.Method == http.MethodPut {
			if peer != nil {
				_ = apiSendObj(out, req, http.StatusOK, peer)
			} else if len(queriedStr) > 0 {
				_ = apiSendObj(out, req, http.StatusNotFound, &APIErr{"peer not found"})
			} else {
				_ = apiSendObj(out, req, http.StatusOK, peers)
			}
		} else {
			out.Header().Set("Allow", "GET, HEAD, POST, PUT")
			_ = apiSendObj(out, req, http.StatusMethodNotAllowed, &APIErr{"unsupported method"})
		}
	})

	smux.HandleFunc("/network/", func(out http.ResponseWriter, req *http.Request) {
		defer func() {
			e := recover()
			if e != nil {
				_ = apiSendObj(out, req, http.StatusInternalServerError, &APIErr{"caught unexpected error in request handler"})
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
				_ = apiSendObj(out, req, http.StatusBadRequest, &APIErr{"only specific networks can be deleted"})
				return
			}
			networks := node.Networks()
			for _, nw := range networks {
				if nw.id == queriedID {
					_ = node.Leave(queriedID)
					_ = apiSendObj(out, req, http.StatusOK, apiNetworkFromNetwork(nw))
					return
				}
			}
			_ = apiSendObj(out, req, http.StatusNotFound, &APIErr{"network not found"})
		} else if req.Method == http.MethodPost || req.Method == http.MethodPut {
			if queriedID == 0 {
				_ = apiSendObj(out, req, http.StatusBadRequest, nil)
				return
			}
			var nw APINetwork
			if apiReadObj(out, req, &nw) == nil {
				n := node.GetNetwork(nw.ID)
				if n == nil {
					if nw.ControllerFingerprint != nil && nw.ControllerFingerprint.Address != nw.ID.Controller() {
						_ = apiSendObj(out, req, http.StatusBadRequest, &APIErr{"fingerprint's address does not match what should be the controller's address"})
					} else {
						n, err := node.Join(nw.ID, nw.ControllerFingerprint, nw.Settings, nil)
						if err != nil {
							_ = apiSendObj(out, req, http.StatusBadRequest, &APIErr{"only individual networks can be added or modified with POST/PUT"})
						} else {
							_ = apiSendObj(out, req, http.StatusOK, apiNetworkFromNetwork(n))
						}
					}
				} else {
					if nw.Settings != nil {
						n.SetLocalSettings(nw.Settings)
					}
					_ = apiSendObj(out, req, http.StatusOK, apiNetworkFromNetwork(n))
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
				return
			}
			for _, nw := range networks {
				if nw.ID() == queriedID {
					_ = apiSendObj(out, req, http.StatusOK, apiNetworkFromNetwork(nw))
					return
				}
			}
			_ = apiSendObj(out, req, http.StatusNotFound, &APIErr{"network not found"})
		} else {
			out.Header().Set("Allow", "GET, HEAD, PUT, POST, DELETE")
			_ = apiSendObj(out, req, http.StatusMethodNotAllowed, &APIErr{"unsupported method " + req.Method})
		}
	})

	listener, err := createNamedSocketListener(basePath, APISocketName)
	if err != nil {
		return nil, nil, err
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
		_ = httpServer.Serve(listener)
		_ = listener.Close()
	}()

	var tcpHttpServer *http.Server
	tcpBindAddr := node.LocalConfig().Settings.APITCPBindAddress
	if tcpBindAddr != nil {
		tcpListener, err := net.ListenTCP("tcp", &net.TCPAddr{
			IP:   tcpBindAddr.IP,
			Port: tcpBindAddr.Port,
		})
		if err != nil {
			node.infoLog.Printf("ERROR: unable to start API HTTP server at TCP bind address %s: %s (named socket listener startd, continuing anyway)", tcpBindAddr.String(), err.Error())
		} else {
			tcpHttpServer = &http.Server{
				MaxHeaderBytes: 4096,
				Handler:        smux,
				IdleTimeout:    10 * time.Second,
				ReadTimeout:    10 * time.Second,
				WriteTimeout:   600 * time.Second,
			}
			tcpHttpServer.SetKeepAlivesEnabled(true)
			go func() {
				_ = tcpHttpServer.Serve(tcpListener)
				_ = tcpListener.Close()
			}()
		}
	}

	return httpServer, tcpHttpServer, nil
}
