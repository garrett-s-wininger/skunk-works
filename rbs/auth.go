package main

import (
	"crypto/rand"
	"fmt"
	"github.com/garrett-s-wininger/skunk-works/rbs/oidc"
	"log"
	"net/http"
	"strings"
	"sync"
)

type AuthConfig struct {
	AuthN   AuthNProvider
	Session SessionProvider
}

type SessionProvider interface {
	Authenticated(id string) (bool, error)
	Create() (string, error)
	Downgrade(id string) (string, error)
	Exists(id string) bool
	Upgrade(id string) (string, error)
}

type Session struct {
	authenticated bool
	// TODO(garrett): Needs expiry
	// TODO(garrett): Add user information
	// TODO(garrett): Make this available to templates
	id string
}

type InMemorySessionManager struct {
	mutex    sync.Mutex
	sessions map[string]*Session
}

func (m *InMemorySessionManager) Authenticated(id string) (bool, error) {
	m.mutex.Lock()
	defer m.mutex.Unlock()

	session := m.sessions[id]

	if session == nil || !session.authenticated {
		return false, nil
	}

	return true, nil
}

func (m *InMemorySessionManager) Create() (string, error) {
	id := rand.Text()

	m.mutex.Lock()
	m.sessions[id] = &Session{false, id}
	m.mutex.Unlock()

	return id, nil
}

func (m *InMemorySessionManager) Downgrade(id string) (string, error) {
	newSessionID := rand.Text()

	m.mutex.Lock()
	defer m.mutex.Unlock()

	session := m.sessions[id]

	if session == nil {
		return "", fmt.Errorf("Requested session for downgrade could not be found")
	}

	delete(m.sessions, id)

	m.sessions[newSessionID] = &Session{false, newSessionID}
	return newSessionID, nil
}

func (m *InMemorySessionManager) Exists(id string) bool {
	m.mutex.Lock()
	defer m.mutex.Unlock()

	return m.sessions[id] != nil
}

func (m *InMemorySessionManager) Upgrade(id string) (string, error) {
	newSessionID := rand.Text()

	m.mutex.Lock()
	defer m.mutex.Unlock()

	session := m.sessions[id]

	if session == nil {
		return "", fmt.Errorf("Requested session for upgrade could not be found")
	}

	delete(m.sessions, id)

	m.sessions[newSessionID] = &Session{true, newSessionID}
	return newSessionID, nil
}

type AuthNProvider interface {
	Authenticate(w http.ResponseWriter, r *http.Request, next http.Handler)
	RegisterHandlers(*http.ServeMux)
	// TODO(garrett): Add method to retrieve session manager to enforce we have it available
}

type AnonymousAuthN struct {
	sessionManager SessionProvider
}

func (m *AnonymousAuthN) Authenticate(w http.ResponseWriter, r *http.Request, next http.Handler) {
	currentSession, err := r.Cookie("id")

	if err != nil {
		http.Error(w, "Attempted authentication on request that is not session managed", http.StatusUnauthorized)
		return
	}

	sessionID, err := m.sessionManager.Upgrade(currentSession.Value)

	if err != nil {
		http.Error(w, "Failed to upgrade session to authenticated", http.StatusInternalServerError)
		return
	}

	// TODO(garrett): Migrate to logging subsystem
	log.Println("Upgraded session from anonymous to authenticated")

	// TODO(garrett): Evaluate additional session cookie flags
	// NOTE(garrett): Likely should force secure but this makes local dev difficult.
	// We should explore options here, possibly setting secure only if we're listening
	// with TLS
	w.Header().Set("Set-Cookie", fmt.Sprintf("id=%s; HttpOnly; Path=/", sessionID))

	if next != nil {
		next.ServeHTTP(w, r)
	}
}

func (*AnonymousAuthN) RegisterHandlers(*http.ServeMux) {}

type OIDCAuthN struct {
	config         oidc.Configuration
	sessionManager SessionProvider
}

func (m *OIDCAuthN) RegisterHandlers(mux *http.ServeMux) {
	oidcCallbackChain := NewMiddlewareChain(
		NewAllowedMethodFilter([]string{http.MethodGet, http.MethodHead}),
		// FIXME(garrett): Properly route to the next middleware handler
		&PassThrough{&OIDCCallbackHandler{m.config, m.sessionManager}},
	)

	mux.Handle("/oidc/callback", oidcCallbackChain)
	mux.Handle("/oidc/callback/{$}", oidcCallbackChain)
}

func (m *OIDCAuthN) Authenticate(w http.ResponseWriter, r *http.Request, next http.Handler) {
	redirectSettings := oidc.RedirectSettings{
		CallbackURI:  m.config.ApplicationURI.JoinPath("oidc", "callback"),
		AuthEndpoint: m.config.OIDCDomain.JoinPath("oauth2", "v1", "authorize"),
		ClientID:     m.config.OIDCClientID,
		// TODO(garrett): This value should be cryptographically bound to a session
		State: r.URL.String(),
	}

	oidc.Redirect(w, r, redirectSettings)
}

type OIDCCallbackHandler struct {
	config         oidc.Configuration
	sessionManager SessionProvider
}

func (h *OIDCCallbackHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	currentSession, err := r.Cookie("id")

	if err != nil {
		http.Error(w, "OIDC handshake attempted without a configured session", http.StatusUnauthorized)
		return
	}

	authorizationCode := r.FormValue("code")

	if len(authorizationCode) == 0 {
		http.Error(w, "Callback received without auth code", http.StatusBadRequest)
		return
	}

	tokenSettings := oidc.TokenRequestSettings{
		CallbackURI:   h.config.ApplicationURI.JoinPath("oidc", "callback"),
		TokenEndpoint: h.config.OIDCDomain.JoinPath("oauth2", "v1", "token"),
		AuthCode:      r.FormValue("code"),
		ClientID:      h.config.OIDCClientID,
		ClientSecret:  h.config.OIDCClientSecret,
	}

	token, err := oidc.RequestToken(tokenSettings)

	if err != nil {
		http.Error(w, "Unable to perform OIDC token request", http.StatusInternalServerError)
		return
	}

	// NOTE(garrett): This presumes we actually have a JWKS, it's the case for Okta but not
	// the only potential avenue according to the OIDC spec (ex. JWK specified, symmetric signing)
	keys, err := oidc.RequestJWKS(h.config.OIDCDomain.JoinPath("oauth2", "v1", "keys"))

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
				Issuer:   h.config.OIDCDomain.String(),
				Audience: h.config.OIDCDomain.String(),
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

	updatedSessionID, err := h.sessionManager.Upgrade(currentSession.Value)

	if err != nil {
		http.Error(w, "Failed to upgrade session to authenticated", http.StatusInternalServerError)
		return
	}

	// TODO(garrett): Migrate to logging subsystem
	w.Header().Set("Set-Cookie", fmt.Sprintf("id=%s; HttpOnly; Path=/", updatedSessionID))
	log.Println("Successfully upgraded session to authenticated")

	// FIXME(garrett): Validate code against session information, HS256 w/ session as key?
	requestedEndpoint := r.FormValue("state")

	if requestedEndpoint == "" {
		http.Error(w, "Attempted callback without proper OIDC state", http.StatusBadRequest)
		return
	}

	http.Redirect(w, r, requestedEndpoint, http.StatusFound)
}
