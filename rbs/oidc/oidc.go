package oidc

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"net/url"
)

const (
	AlgorithmRS256 = "RS256"
	KeyTypeRSA = "RSA"
	KeyUsageSigning = "sig"
)

type AccessTokenResponse struct {
	TokenType string `json:"token_type"`
	ExpirySeconds int `json:"expires_in"`
	AccessToken string `json:"access_token"`
}

type JWK struct {
	Type string `json:"kty"`
	Use string `json:"use",omitempty`
	Algorithm string `json:"alg",omitempty`
	Modulus string `json:"n",omitempty`
	Exponent string `json:"e",omitempty`
}

type JWKS struct {
	Keys []JWK `json:"keys"`
}

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

func RequestJWKS(endpoint *url.URL) (JWKS, error) {
	// We're already receiving a good URL, no point in validating
	req, _ := http.NewRequest(
		http.MethodGet,
		endpoint.String(),
		bytes.NewBuffer([]byte{}))

	req.Header.Add("Accept", "application/json")
	client := &http.Client{}
	resp, err := client.Do(req)

	if err != nil {
		return JWKS{}, err
	}

	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return JWKS{}, fmt.Errorf("Request for JWKS failed")
	}

	content, err := ioutil.ReadAll(resp.Body)

	if err != nil {
		return JWKS{}, err
	}

	var keySet JWKS

	err = json.Unmarshal(content, &keySet)

	if err != nil {
		return JWKS{}, err
	}

	return keySet, nil
}

func RequestToken(settings TokenRequestSettings) (AccessTokenResponse, error) {
	queryParameters := url.Values{}
	queryParameters.Add("grant_type", "authorization_code")
	queryParameters.Add("code", settings.AuthCode)
	queryParameters.Add("redirect_uri", settings.CallbackURI.String())

	oidcTokenEndpoint := *settings.TokenEndpoint
	oidcTokenEndpoint.RawQuery = queryParameters.Encode()

	// We're already receiving a good URL, no point in validating
	req, _ := http.NewRequest(
		http.MethodPost,
		oidcTokenEndpoint.String(),
		bytes.NewBuffer([]byte{}))

	req.Header.Add("Content-Type", "application/x-www-form-urlencoded")
	req.SetBasicAuth(
		url.QueryEscape(settings.ClientID),
		url.QueryEscape(settings.ClientSecret))

	client := &http.Client{}
	resp, err := client.Do(req)

	if err != nil {
		return AccessTokenResponse{}, err
	}

	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return AccessTokenResponse{}, fmt.Errorf("Request for OIDC access token failed")
	}

	content, err := ioutil.ReadAll(resp.Body)

	if err != nil {
		return AccessTokenResponse{}, err
	}

	var accessTokenResponse AccessTokenResponse

	err = json.Unmarshal(content, &accessTokenResponse)

	if err != nil {
		return AccessTokenResponse{}, err
	}

	// TODO(garrett): Validate access token, ID token
	return accessTokenResponse, nil
}
