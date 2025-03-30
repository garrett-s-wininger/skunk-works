package oidc

import (
	"bytes"
	"net/http"
	"net/url"
)

type RedirectSettings struct {
	CallbackURI *url.URL
	AuthEndpoint *url.URL
	ClientID string
	State string
}

type TokenRequestSettings struct {
	CallbackURI *url.URL
	TokenEndpoint *url.URL
	AuthCode string
	ClientID string
	ClientSecret string
}

func Redirect(
		w http.ResponseWriter,
		r *http.Request,
		settings RedirectSettings) {
	queryParameters := url.Values{}
	queryParameters.Add("response_type", "code")
	queryParameters.Add("scope", "openid profile")
	queryParameters.Add("client_id", settings.ClientID)
	queryParameters.Add("state", settings.State)
	queryParameters.Add("redirect_uri", settings.CallbackURI.String())

	oidcAuthEndpoint := *settings.AuthEndpoint
	oidcAuthEndpoint.RawQuery = queryParameters.Encode()
	http.Redirect(w, r, oidcAuthEndpoint.String(), http.StatusFound)
}

func RequestJWKS(endpoint *url.URL) (*http.Response, error) {
	req, err := http.NewRequest(
		"GET",
		endpoint.String(),
		bytes.NewBuffer([]byte{}))

	if err != nil {
		return nil, err
	}

	req.Header.Add("Accept", "application/json")
	client := &http.Client{}

	return client.Do(req)
}

func RequestToken(settings TokenRequestSettings) (*http.Response, error) {
	queryParameters := url.Values{}
	queryParameters.Add("grant_type", "authorization_code")
	queryParameters.Add("code", settings.AuthCode)
	queryParameters.Add("redirect_uri", settings.CallbackURI.String())

	oidcTokenEndpoint := *settings.TokenEndpoint
	oidcTokenEndpoint.RawQuery = queryParameters.Encode()

	req, err := http.NewRequest(
		"POST",
		oidcTokenEndpoint.String(),
		bytes.NewBuffer([]byte{}))

	if err != nil {
		return nil, err
	}

	req.Header.Add("Content-Type", "application/x-www-form-urlencoded")
	req.SetBasicAuth(
		url.QueryEscape(settings.ClientID),
		url.QueryEscape(settings.ClientSecret))

	client := &http.Client{}
	return client.Do(req)
}
