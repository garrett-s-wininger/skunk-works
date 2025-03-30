package main

import (
	"crypto/rand"
	"flag"
	"fmt"
	"html/template"
	"log"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strings"
)

var requiredConfigs = []string{
	"applicationUri",
	"oidcClientId",
	"oidcDomain",
}

var serverConfig map[string]string

func index(w http.ResponseWriter, r *http.Request) {
	paths := []string{
		filepath.Join("templates/index.tmpl"),
	}

	templ := template.Must(template.ParseFiles(paths...))
	err := templ.Execute(w, nil)

	if err != nil {
		log.Fatal("Template execution failure: %s", err)
	}
}

func login(w http.ResponseWriter, r *http.Request) {
	oidcAuthEndpoint, err := url.Parse(serverConfig["oidcDomain"])

	if err != nil {
		http.Error(w, "Invalid OIDC provider domain", http.StatusInternalServerError)
		return
	}

	callbackUri, err := url.Parse(serverConfig["applicationUri"])

	if err != nil {
		http.Error(w, "Invalid application URI", http.StatusInternalServerError)
		return
	}

	queryParameters := url.Values{}
	queryParameters.Add("response_type", "code")
	queryParameters.Add("scope", "openid profile")
	queryParameters.Add("client_id", serverConfig["oidcClientId"])

	// TODO(garrett): This value should be bound to a session
	queryParameters.Add("state", rand.Text())

	callbackUri = callbackUri.JoinPath("oidc", "callback")
	queryParameters.Add("redirect_uri", callbackUri.String())

	oidcAuthEndpoint = oidcAuthEndpoint.JoinPath("oauth2", "v1", "authorize")
	oidcAuthEndpoint.RawQuery = queryParameters.Encode()
	http.Redirect(w, r, oidcAuthEndpoint.String(), http.StatusFound)
}

func loginCallback(w http.ResponseWriter, r *http.Request) {
	fmt.Fprintf(w, "Received authorization code from IdP!")
	// TODO(garrett): Use authorization code to obtain token
	// TODO(garrett): Use token to obtain userinfo
	// TODO(garrett): Implement session tracking
}

func parseConfiguration() (map[string]string, error) {
	// TODO(garrett): This is ridiculously expensive, just handroll a strict parser
	contents, err := os.ReadFile(".env")

	if err != nil {
		return nil, err
	}

	stringData := string(contents)
	configMap := make(map[string]string)

	for _, line := range strings.Split(strings.ReplaceAll(stringData, "\r\n", "\n"), "\n") {
		if len(line) == 0 {
			continue
		}

		keyValuePair := strings.Split(line, "=")

		if len(keyValuePair) != 2 {
			fmt.Printf("%s: %d\n", line, len(keyValuePair))
			return nil, fmt.Errorf("configuration: malformed line detected %s", line)
		}

		configMap[keyValuePair[0]] = keyValuePair[1]
	}

	for _, key := range requiredConfigs {
		_, ok := configMap[key]

		if !ok {
			return nil, fmt.Errorf("configuration: required config %s is missing from .env", key)
		}
	}

	return configMap, nil
}

func main() {
	port := flag.Int("p", 8080, "Requested port number to listen on")
	flag.Parse()

	config, err := parseConfiguration()

	if err != nil {
		log.Fatal(err)
	}

	serverConfig = config
	mux := http.NewServeMux()

	mux.HandleFunc("/{$}", index)
	mux.HandleFunc("/login/{$}", login)
	mux.HandleFunc("/oidc/callback/{$}", loginCallback)
	mux.Handle(
		"/static/",
		http.StripPrefix("/static/", http.FileServer(http.Dir("./static"))))

	listenAddress := fmt.Sprintf("0.0.0.0:%d", *port)
	log.Fatal(http.ListenAndServe(listenAddress, mux))
}
