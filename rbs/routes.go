package main

import (
	"crypto/rand"
	"github.com/garrett-s-wininger/skunk-works/rbs/oidc"
	"html/template"
	"log"
	"net/http"
	"path/filepath"
	"strings"
)

func getApplicationRoutes() *http.ServeMux {
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

	return mux
}

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

	// NOTE(garrett): This presumes we actually have a JWKS, it's the case for Okta but not
	// the only potential avenue according to the OIDC spec (ex. JWK specified, symmetric signing)
	keys, err := oidc.RequestJWKS(serverConfig.OIDCDomain.JoinPath("oauth2", "v1", "keys"))

	if err != nil {
		http.Error(w, "Unable to perform JWKS retrieval", http.StatusInternalServerError)
		return
	}

	endOfHeader := strings.Index(token.AccessToken, ".")

	if endOfHeader == -1 {
		http.Error(w, "Received token cannot possibly be valid", http.StatusInternalServerError)
		return
	}

	joseHeader, err := oidc.DecodeJOSEHeader(token.AccessToken[0:endOfHeader])

	if err != nil {
		http.Error(w, "Unable to parse JOSE header from OIDC token", http.StatusInternalServerError)
		return
	}

	if joseHeader.Type != oidc.JOSETypeOktaAccessToken {
		http.Error(w, "Decoded JOSE header indicates payload is not a JWT", http.StatusInternalServerError)
		return
	}

	successfulValidation := false

	for _, key := range keys.Keys {
		// NOTE(garrett): We're hardcoding RS256 here but there are more in the spec, this one is required
		if key.Type != oidc.KeyTypeRSA {
			continue
		}

		if key.Algorithm != "" && key.Algorithm != oidc.AlgorithmRS256 {
			continue
		}

		if key.Use != "" && key.Use != oidc.KeyUsageSigning {
			continue
		}

		if joseHeader.ID != "" && key.ID != "" && key.ID != joseHeader.ID {
			continue
		}

		data, err := oidc.DecodeJWT(token.AccessToken, key.Modulus, key.Exponent)

		if err != nil {
			continue
		}

		successfulValidation = oidc.ValidateJWT(
			data,
			oidc.JWTClaimExpectations{
				Issuer:   serverConfig.OIDCDomain.String(),
				Audience: serverConfig.OIDCDomain.String(),
			},
		)

		break
	}

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
	http.Redirect(w, r, "/", http.StatusFound)
}
