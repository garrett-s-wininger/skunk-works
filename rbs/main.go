package main

import (
	"flag"
	"fmt"
	"log"
	"net/http"
)

var serverConfig ServerConfig

func main() {
	port := flag.Int("p", 8080, "Requested port number to listen on")
	cert := flag.String("c", "", "Location of certificate for HTTP2/TLS serving")
	key := flag.String("k", "", "Location of private key for HTTP2/TLS service")
	flag.Parse()

	config, err := parseConfiguration()

	if err != nil {
		log.Fatal(err)
	}

	serverConfig = config
	mux := http.NewServeMux()
	allowGetAndHead := NewAllowedMethods(http.MethodGet, http.MethodHead)

	indexChain := &MiddlewareChain{
		[]Middleware{
			allowGetAndHead,
			&TerminalHandler{http.HandlerFunc(index)},
		},
	}

	mux.Handle("/{$}", indexChain)

	loginChain := &MiddlewareChain{
		[]Middleware{
			allowGetAndHead,
			&TerminalHandler{http.HandlerFunc(login)},
		},
	}

	mux.Handle("/login", loginChain)
	mux.Handle("/login/{$}", loginChain)

	oidcCallbackChain := &MiddlewareChain{
		[]Middleware{
			allowGetAndHead,
			&TerminalHandler{http.HandlerFunc(loginCallback)},
		},
	}

	mux.Handle("/oidc/callback", oidcCallbackChain)
	mux.Handle("/oidc/callback/{$}", oidcCallbackChain)

	mux.Handle(
		"/static/",
		http.StripPrefix("/static/", http.FileServer(http.Dir("./static"))))

	listenAddress := fmt.Sprintf("0.0.0.0:%d", *port)

	if (*cert == "" && *key != "") || (*cert != "" && *key == "") {
		log.Fatal("Detected HTTP2/TLS settings, both certificate (c) and key (k) must be provided")
	}

	// TODO(garrett): Check on the possibility of automatically reloading the TLS context to prevent
	// necessity for application restart (FSEvents/inotify/FileWatch)

	if (*cert == "" && *key == "") {
		log.Fatal(http.ListenAndServe(listenAddress, mux))
	} else {
		log.Fatal(http.ListenAndServeTLS(listenAddress, *cert, *key, mux))
	}
}
