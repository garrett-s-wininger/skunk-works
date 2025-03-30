package main

import (
	"bytes"
	"crypto/rand"
	"flag"
	"fmt"
	"html/template"
	"io/ioutil"
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
	"oidcClientSecret",
	"oidcDomain",
}

type ServerConfig struct {
	ApplicationUri *url.URL
	OidcClientId string
	OidcClientSecret string // TODO(garrett): Investigate SGX or similar storage
	OidcDomain *url.URL
}

var serverConfig ServerConfig

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
	queryParameters := url.Values{}
	queryParameters.Add("response_type", "code")
	queryParameters.Add("scope", "openid profile")
	queryParameters.Add("client_id", serverConfig.OidcClientId)

	// TODO(garrett): This value should be bound to a session
	queryParameters.Add("state", rand.Text())

	callbackUri := serverConfig.ApplicationUri.JoinPath("oidc", "callback")
	queryParameters.Add("redirect_uri", callbackUri.String())

	oidcAuthEndpoint := serverConfig.OidcDomain.JoinPath("oauth2", "v1", "authorize")
	oidcAuthEndpoint.RawQuery = queryParameters.Encode()
	http.Redirect(w, r, oidcAuthEndpoint.String(), http.StatusFound)
}

func loginCallback(w http.ResponseWriter, r *http.Request) {
	// TODO(garrett): Split into token separate token/JWKS retrieval methods, this is getting large
	// TODO(garrett): Validate returned state against session management to ensure we sent it
	queryParameters := url.Values{}
	queryParameters.Add("grant_type", "authorization_code")
	queryParameters.Add("code", r.FormValue("code"))

	callbackUri := serverConfig.ApplicationUri.JoinPath("oidc", "callback")
	queryParameters.Add("redirect_uri", callbackUri.String())

	oidcTokenEndpoint := serverConfig.OidcDomain.JoinPath("oauth2", "v1", "token")
	oidcTokenEndpoint.RawQuery = queryParameters.Encode()

	tokenRequest, err := http.NewRequest(
		"POST",
		oidcTokenEndpoint.String(),
		bytes.NewBuffer([]byte{}))

	if err != nil {
		http.Error(w, "Unable to create token request", http.StatusInternalServerError)
		return
	}

	tokenRequest.Header.Add("Content-Type", "application/x-www-form-urlencoded")
	tokenRequest.SetBasicAuth(
		url.QueryEscape(serverConfig.OidcClientId),
		url.QueryEscape(serverConfig.OidcClientSecret))

	client := &http.Client{}
	resp, err := client.Do(tokenRequest)

	if err != nil {
		http.Error(w, "Unable to perform OIDC token request", http.StatusInternalServerError)
		return
	}

	defer resp.Body.Close()

	body, err := ioutil.ReadAll(resp.Body)

	if err != nil {
		http.Error(w, "Unable to read token response body", http.StatusInternalServerError)
		return
	}

	// TODO(garrett): Handle error response from IdP, ala invalid Client Secret
	// TODO(garrett): Remove, outputs sensitive data
	log.Println(string(body))

	jwksRequest, err := http.NewRequest(
		"GET",
		serverConfig.OidcDomain.JoinPath("oauth2", "v1", "keys").String(),
		bytes.NewBuffer([]byte{}))

	if err != nil {
		http.Error(w, "Unable to create JWKS request", http.StatusInternalServerError)
		return
	}

	jwksRequest.Header.Add("Accept", "application/json")
	keyResponse, err := client.Do(jwksRequest)

	if err != nil {
		http.Error(w, "Unable to perform JWKS retrieval", http.StatusInternalServerError)
		return
	}

	defer keyResponse.Body.Close()

	body, err = ioutil.ReadAll(keyResponse.Body)

	if err != nil {
		http.Error(w, "Failed to read JWKS response body", http.StatusInternalServerError)
		return
	}

	// TODO(garrett) Validate token data, decrypt from JWKS result
	// TODO(garrett): Remove, outputs sensitive data
	log.Println(string(body))

	// TODO(garrett): Implement session tracking
}

func parseConfiguration() (ServerConfig, error) {
	// TODO(garrett): This is function is not perf-aware, should be
	contents, err := os.ReadFile(".env")

	if err != nil {
		return ServerConfig{}, err
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
			return ServerConfig{}, fmt.Errorf("configuration: malformed line detected %s", line)
		}

		configMap[keyValuePair[0]] = keyValuePair[1]
	}

	for _, key := range requiredConfigs {
		_, ok := configMap[key]

		if !ok {
			return ServerConfig{}, fmt.Errorf("configuration: required config %s is missing from .env", key)
		}
	}


	applicationUri, err := url.Parse(configMap["applicationUri"])

	if err != nil {
		return ServerConfig{}, fmt.Errorf("configuration: applicationUri is not a valid URI")
	}

	oidcDomain, err := url.Parse(configMap["oidcDomain"])

	if err != nil {
		return ServerConfig{}, fmt.Errorf("configuration: oidcDomain is not a valid URI")
	}

	parsedConfig := ServerConfig{
		ApplicationUri: applicationUri,
		OidcClientId: configMap["oidcClientId"],
		OidcClientSecret: configMap["oidcClientSecret"],
		OidcDomain: oidcDomain,
	}

	return parsedConfig, nil
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
	mux.HandleFunc("/login", login)
	mux.HandleFunc("/login/{$}", login)
	mux.HandleFunc("/oidc/callback", loginCallback)
	mux.HandleFunc("/oidc/callback/{$}", loginCallback)
	mux.Handle(
		"/static/",
		http.StripPrefix("/static/", http.FileServer(http.Dir("./static"))))

	listenAddress := fmt.Sprintf("0.0.0.0:%d", *port)
	log.Fatal(http.ListenAndServe(listenAddress, mux))
}
