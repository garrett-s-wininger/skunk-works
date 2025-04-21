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
	"math/big"
	"net/http"
	"net/url"
	"strings"
	"time"
)

const (
	AlgorithmRS256 = "RS256"
	// NOTE(garrett): Apparently Okta uses a specifically defined type here,
	// should be on the lookout for others, as OIDC doesn't appear to standardize
	JOSETypeOktaAccessToken = "application/okta-internal-at+jwt"
	KeyTypeRSA = "RSA"
	KeyUsageSigning = "sig"
)

var JOSEBase64 = base64.URLEncoding.WithPadding(base64.NoPadding)

type AccessTokenResponse struct {
	TokenType string `json:"token_type"`
	ExpirySeconds int `json:"expires_in"`
	AccessToken string `json:"access_token"`
}

type Configuration struct {
	ApplicationURI   *url.URL
	OIDCClientID     string
	OIDCClientSecret string // TODO(garrett): Investigate SGX or similar storage
	OIDCDomain       *url.URL
}

type JOSE struct {
	Algorithm string `json:"alg"`
	Type string `json:"typ"`
	ID string `json:"kid",omitempty`
	Encryption string `json:"enc",omitempty`
}

type JWK struct {
	Type string `json:"kty"`
	ID string `json:"kid",omitempty`
	Use string `json:"use",omitempty`
	Algorithm string `json:"alg",omitempty`
	Modulus string `json:"n",omitempty`
	Exponent string `json:"e",omitempty`
}

type JWKS struct {
	Keys []JWK `json:"keys"`
}

type JWT struct {
	ID string `json:"jti",omitempty`
	Issuer string `json:"iss",omitempty`
	IssuedAt int64 `json:"iat",omitempty`
	Subject string `json:"sub",omitempty`
	Audience string `json:"aud",omitempty`
	Expiration int64 `json:"exp",omitempty`
}

type JWTClaimExpectations struct {
	Issuer string
	Audience string
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

func DecodeSignedJWTPayload(jwtWithoutHeader string) ([]byte, []byte, error) {
	jwtBase64, signatureBase64, found := strings.Cut(jwtWithoutHeader, ".")

	if !found {
		return nil, nil, fmt.Errorf("oidc: input is not a valid JWT payload + signature")
	}

	jwtBytes, err := JOSEBase64.DecodeString(jwtBase64)

	if err != nil {
		return nil, nil, err
	}

	signatureBytes, err := JOSEBase64.DecodeString(signatureBase64)

	if err != nil {
		return nil, nil, err
	}

	return jwtBytes, signatureBytes, nil
}

func DecodeJWT(jwt string, modulus string, exponent string) (JWT, error) {
	headerSize := strings.Index(jwt, ".")
	jose, err := DecodeJOSEHeader(jwt[0:headerSize])

	if err != nil {
		return JWT{}, err
	}

	if jose.Algorithm != AlgorithmRS256 {
		return JWT{}, fmt.Errorf("oidc: unsigned, encrypted, or unsupported algorithm detected in JOSE header")
	}

	jwtBytes, signature, err := DecodeSignedJWTPayload(jwt[headerSize+1:])

	if err != nil {
		return JWT{}, err
	}

	modBytes, err := JOSEBase64.DecodeString(modulus)

	if err != nil {
		return JWT{}, err
	}

	exponentBytes, err := JOSEBase64.DecodeString(exponent)

	if err != nil {
		return JWT{}, err
	}

	publicKey := &rsa.PublicKey{
		N: new(big.Int).SetBytes(modBytes),
		E: int(new(big.Int).SetBytes(exponentBytes).Int64()),
	}

	signedContentLen := strings.LastIndex(jwt, ".")
	hash := sha256.New()
	hash.Write([]byte(jwt[0:signedContentLen]))

	err = rsa.VerifyPKCS1v15(publicKey, crypto.SHA256, hash.Sum(nil), signature)

	if err != nil {
		return JWT{}, err
	}

	var jwtStruct JWT
	err = json.Unmarshal(jwtBytes, &jwtStruct)

	if err != nil {
		return JWT{}, nil
	}

	return jwtStruct, nil
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
	// NOTE(garrett): We're already receiving a good URL, no point in validating
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

	// NOTE(garrett): We're already receiving a good URL, no point in validating
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

	return accessTokenResponse, nil
}

func ValidateJWT(jwt JWT, expectations JWTClaimExpectations) bool {
	if jwt.Issuer == "" || jwt.Issuer != expectations.Issuer {
		return false
	}

	if jwt.Audience == "" || jwt.Audience != expectations.Audience {
		return false
	}

	// NOTE(garrett): We're not currently handling extensions, otherwise would need to validate "azp"
	// NOTE(garrett): We're allowed to use TLS server validation instead of the signing validation
	// but opt not to

	// NOTE(garrett): We already hardcode RS256 support, could later expand to suppor registration value

	if jwt.Expiration == 0 {
		return false
	}

	now := time.Now()

	if now.After(time.Unix(jwt.Expiration, 0)) {
		return false
	}

	// TODO(garrett): Validate issued at time isn't too far behind, our choice on time period
	if jwt.IssuedAt == 0 {
		return false
	}

	difference := now.Sub(time.Unix(jwt.IssuedAt, 0))

	// NOTE(garrett): We're hardcoding 5 minutes, the default max for Kerberos. Perhaps we
	// should make this configurable in the future.
	if difference.Minutes() >= 5 {
		return false
	}

	// NOTE(garrett): We don't send a nonce but if we did, need to validate it here
	// NOTE(garrett): We don't request "acr" claims but if we did, we would need to assert the value
	// NOTE(garrett): We don't request "auth_time" but we did, should check time since last auth

	return true
}
