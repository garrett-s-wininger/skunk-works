package main

import (
	"fmt"
	"net/url"
	"os"
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

