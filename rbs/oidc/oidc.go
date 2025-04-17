package oidc

import (
	"bytes"
	"crypto"
	"crypto/rsa"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"math/big"
	"net/http"
	"net/url"
	"strings"
)

const (
	AlgorithmRS256 = "RS256"
	KeyTypeRSA = "RSA"
	KeyUsageSigning = "sig"
)

var JOSEBase64 = base64.URLEncoding.WithPadding(base64.NoPadding)

type AccessTokenResponse struct {
	TokenType string `json:"token_type"`
	ExpirySeconds int `json:"expires_in"`
	AccessToken string `json:"access_token"`
}

type JOSE struct {
	ID string `json:"kid",omitempty`
	Algorithm string `json:"alg"`
}

type JWK struct {
	ID string `json:"kid"`
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

func DecodeJOSEHeader(header string) (JOSE, error) {
	joseText, err := JOSEBase64.DecodeString(header)

	if err != nil {
		return JOSE{}, err
	}

	var joseHeader JOSE
	err = json.Unmarshal(joseText, &joseHeader)

	if err != nil {
		return JOSE{}, err
	}

	return joseHeader, nil
}

func DecodeSignedJWTPayload(jwtWithoutHeader string) ([]byte, int, []byte, error) {
	jwtBase64, signatureBase64, found := strings.Cut(jwtWithoutHeader, ".")

	if !found {
		return nil, 0, nil, fmt.Errorf("oidc: input is not a valid JWT payload + signature")
	}

	jwtBytes, err := JOSEBase64.DecodeString(jwtBase64)

	if err != nil {
		return nil, 0, nil, err
	}

	signatureBytes, err := JOSEBase64.DecodeString(signatureBase64)

	if err != nil {
		return nil, 0, nil, err
	}

	return jwtBytes, len(jwtBase64), signatureBytes, nil
}

func DecodeJWT(jwt string, modulus string, exponent string) ([]byte, error) {
	headerSize := strings.Index(jwt, ".")
	jose, err := DecodeJOSEHeader(jwt[0:headerSize])

	if err != nil {
		return nil, err
	}

	if jose.Algorithm != AlgorithmRS256 {
		return nil, fmt.Errorf("oidc: unsigned, encrypted, or unsupported algorithm detected in JOSE header")
	}

	jwtBytes, payloadSize, signature, err := DecodeSignedJWTPayload(jwt[headerSize+1:])

	if err != nil {
		return nil, err
	}

	modBytes, err := JOSEBase64.DecodeString(modulus)

	if err != nil {
		return nil, err
	}

	exponentBytes, err := JOSEBase64.DecodeString(exponent)

	if err != nil {
		return nil, err
	}

	publicKey := &rsa.PublicKey{
		N: new(big.Int).SetBytes(modBytes),
		E: int(new(big.Int).SetBytes(exponentBytes).Int64()),
	}

	signedContentLen := headerSize + payloadSize + 1
	hash := sha256.New()
	hash.Write([]byte(jwt[0:signedContentLen]))

	err = rsa.VerifyPKCS1v15(publicKey, crypto.SHA256, hash.Sum(nil), signature)

	if err != nil {
		return nil, err
	}

	return jwtBytes, nil
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
