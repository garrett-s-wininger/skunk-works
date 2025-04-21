package main

import (
	"crypto/tls"
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"sync/atomic"
	"time"
)

var keyingMaterial atomic.Value

func periodicCryptoMaterialRefresh(cert, key string, interval time.Duration) {
	certMTime := time.Now()
	keyMTime := time.Now()
	ticker := time.NewTicker(interval)

	for range ticker.C {
		certFileInfo, err := os.Stat(cert)

		if err != nil {
			log.Println("WARN: Failed to stat certificate chain for refresh")
			continue
		}

		keyFileInfo, err := os.Stat(key)

		if err != nil {
			log.Println("WARN: Failed to stat certificate key for refresh")
			continue
		}

		newCertMTime := certFileInfo.ModTime()
		newKeyMTime := keyFileInfo.ModTime()

		if newCertMTime.After(certMTime) && newKeyMTime.After(keyMTime) {
			keyPair, err := tls.LoadX509KeyPair(cert, key)

			if err != nil {
				log.Println("WARN: Failed to update key pair with new crypto material")
				continue
			}

			certMTime = newCertMTime
			keyMTime = newKeyMTime
			keyingMaterial.Store(&keyPair)
			log.Println("INFO: Performed keying material refresh for TLS connections")
		}
	}
}

func main() {
	port := flag.Int("p", 8080, "Requested port number to listen on")
	cert := flag.String("c", "", "Location of certificate for HTTP2/TLS serving")
	key := flag.String("k", "", "Location of private key for HTTP2/TLS service")
	flag.Parse()

	config, err := parseConfiguration()

	if err != nil {
		log.Fatal(err)
	}

	mux := getApplicationRoutes(config.auth)
	listenAddress := fmt.Sprintf("0.0.0.0:%d", *port)

	if (*cert == "" && *key != "") || (*cert != "" && *key == "") {
		log.Fatal("Detected partial HTTP2/TLS settings, both certificate (c) and key (k) must be provided")
	}

	if *cert == "" && *key == "" {
		log.Fatal(http.ListenAndServe(listenAddress, mux))
	} else {
		keyPair, err := tls.LoadX509KeyPair(*cert, *key)

		if err != nil {
			log.Fatal(err)
		}

		keyingMaterial.Store(&keyPair)

		tlsConfig := &tls.Config{
			GetCertificate: func(*tls.ClientHelloInfo) (*tls.Certificate, error) {
				return keyingMaterial.Load().(*tls.Certificate), nil
			},
		}

		server := &http.Server{
			Addr:      listenAddress,
			TLSConfig: tlsConfig,
			Handler:   mux,
		}

		// TODO(garrett): Most OSes have methods to get notified on file updates,
		// we should use those instead of polling as there's no need to continously
		// stat the filesystem unless those facilities are unavilable.
		go periodicCryptoMaterialRefresh(*cert, *key, time.Minute)
		log.Fatal(server.ListenAndServeTLS("", ""))
	}
}
