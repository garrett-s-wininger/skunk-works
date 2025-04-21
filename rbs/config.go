package main

import (
	"fmt"
	"github.com/garrett-s-wininger/skunk-works/rbs/oidc"
	"net/url"
	"os"
	"strings"
)

const (
	AuthProviderAnonymous = "anonymous"
	AuthProviderField     = "authProvider"
	AuthProviderOIDC      = "oidc"
	SessionProviderField  = "sessionProvider"
	SessionProviderMemory = "memory"
)

var requiredOIDCOptions = []string{
	"applicationURI",
	"oidcClientID",
	"oidcClientSecret",
	"oidcDomain",
}

var requiredGlobals = []string{
	AuthProviderField,
	SessionProviderField,
}

type ServerConfig struct {
	auth AuthConfig
}

func enforceKeyPresence(parsedKeys map[string]string, requiredKeys []string) error {
	for _, key := range requiredKeys {
		_, ok := parsedKeys[key]

		if !ok {
			return fmt.Errorf("configuration: required config %s is missing from .env", key)
		}
	}

	return nil
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

	err = enforceKeyPresence(configMap, requiredGlobals)

	if err != nil {
		return ServerConfig{}, err
	}

	if configMap[SessionProviderField] != SessionProviderMemory {
		return ServerConfig{}, fmt.Errorf("Unsupported session provider requested: %s", configMap[SessionProviderField])
	}

	sessionProvider := &InMemorySessionManager{
		sessions: make(map[string]*Session),
	}

	var authNProvider AuthNProvider

	if configMap[AuthProviderField] == AuthProviderAnonymous {
		authNProvider = &AnonymousAuthN{sessionProvider}
	} else if configMap[AuthProviderField] == AuthProviderOIDC {
		err = enforceKeyPresence(configMap, requiredOIDCOptions)

		if err != nil {
			return ServerConfig{}, err
		}

		applicationURI, err := url.Parse(configMap["applicationURI"])

		if err != nil {
			return ServerConfig{}, fmt.Errorf("configuration: applicationUri is not a valid URI")
		}

		oidcDomain, err := url.Parse(configMap["oidcDomain"])

		if err != nil {
			return ServerConfig{}, fmt.Errorf("configuration: oidcDomain is not a valid URI")
		}

		oidcConfig := oidc.Configuration{
			ApplicationURI:   applicationURI,
			OIDCClientID:     configMap["oidcClientID"],
			OIDCClientSecret: configMap["oidcClientSecret"],
			OIDCDomain:       oidcDomain,
		}

		authNProvider = &OIDCAuthN{oidcConfig, sessionProvider}
	} else {
		return ServerConfig{}, fmt.Errorf("Unsupported authentication provider requested: %s", configMap[AuthProviderField])
	}

	authConfig := AuthConfig{
		AuthN:   authNProvider,
		Session: sessionProvider,
	}

	return ServerConfig{authConfig}, nil
}
