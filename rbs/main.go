package main

import (
	"crypto/rand"
	"flag"
	"fmt"
	"github.com/garrett-s-wininger/skunk-works/rbs/oidc"
	"html/template"
	"log"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strings"
)

var requiredConfigs = []string{
	"applicationURI",
	"oidcClientID",
	"oidcClientSecret",
	"oidcDomain",
}

type ServerConfig struct {
	ApplicationURI   *url.URL
	OIDCClientID     string
	OIDCClientSecret string // TODO(garrett): Investigate SGX or similar storage
	OIDCDomain       *url.URL
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
	// TODO(garrett): Properly handle non-GET/HEAD calls
	redirectSettings := oidc.RedirectSettings{
		CallbackURI:  serverConfig.ApplicationURI.JoinPath("oidc", "callback"),
		AuthEndpoint: serverConfig.OIDCDomain.JoinPath("oauth2", "v1", "authorize"),
		ClientID:     serverConfig.OIDCClientID,
		// TODO(garrett): This value should be bound to a session
		State: rand.Text(),
	}

	oidc.Redirect(w, r, redirectSettings)
}

func loginCallback(w http.ResponseWriter, r *http.Request) {
	// TODO(garrett): Properly error on non-GET/HEAD calls
	// TODO(garrett): Only allow if not already authenticated
	authorizationCode := r.FormValue("code")

	if len(authorizationCode) == 0 {
		http.Error(w, "Callback received without auth code", http.StatusBadRequest)
		return
	}

	tokenSettings := oidc.TokenRequestSettings{
		CallbackURI:   serverConfig.ApplicationURI.JoinPath("oidc", "callback"),
		TokenEndpoint: serverConfig.OIDCDomain.JoinPath("oauth2", "v1", "token"),
		AuthCode:      r.FormValue("code"),
		ClientID:      serverConfig.OIDCClientID,
		ClientSecret:  serverConfig.OIDCClientSecret,
	}

	token, err := oidc.RequestToken(tokenSettings)

	if err != nil {
		http.Error(w, "Unable to perform OIDC token request", http.StatusInternalServerError)
		return
	}

	keys, err := oidc.RequestJWKS(serverConfig.OIDCDomain.JoinPath("oauth2", "v1", "keys"))

	if err != nil {
		http.Error(w, "Unable to perform JWKS retrieval", http.StatusInternalServerError)
		return
	}

	successfulValidation := false

	// TODO(garrett): Refactor so that when Key ID is present, we only attempt to validate
	// a single signature
	for _, key := range keys.Keys {
		if key.Type == oidc.KeyTypeRSA &&
			key.Algorithm == oidc.AlgorithmRS256 &&
			key.Use == oidc.KeyUsageSigning {

			data, err := oidc.DecodeJWT(token.AccessToken, key.Modulus, key.Exponent)

			if err != nil {
				continue
			}

			log.Println(fmt.Sprintf("Token: %s", data))
			successfulValidation = true
			break
		}
	}

	// TODO(garrett): Perform additional validation IAC OIDC Core specification

	if !successfulValidation {
		// TODO(garrett): Investigate alternative return codes. This seems wrong unless
		// we have bad validation logic. Otherwise, we've received a token we can't validate
		// against the OP which means they're unauthorized, though that isn't the client's
		// fault
		http.Error(w, "Access token could not be verified", http.StatusInternalServerError)
		return
	}

	// TODO(garrett): Implement session tracking
	// TODO(garrett): Implement user creation/matching
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

	applicationURI, err := url.Parse(configMap["applicationURI"])

	if err != nil {
		return ServerConfig{}, fmt.Errorf("configuration: applicationUri is not a valid URI")
	}

	oidcDomain, err := url.Parse(configMap["oidcDomain"])

	if err != nil {
		return ServerConfig{}, fmt.Errorf("configuration: oidcDomain is not a valid URI")
	}

	parsedConfig := ServerConfig{
		ApplicationURI:   applicationURI,
		OIDCClientID:     configMap["oidcClientID"],
		OIDCClientSecret: configMap["oidcClientSecret"],
		OIDCDomain:       oidcDomain,
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
