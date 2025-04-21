package main

import (
	"fmt"
	"log"
	"net/http"
)

type Middleware interface {
	http.Handler
	SetNext(handler http.Handler)
}

type MiddlewareChain struct {
	h http.Handler
}

func (m *MiddlewareChain) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	m.h.ServeHTTP(w, r)
}

// NOTE(garrett): Perhaps receive the final handler first, it reads better this way but not sure...
func NewMiddlewareChain(rootHandler Middleware, supplementalHandlers ...Middleware) *MiddlewareChain {
	handlerCount := len(supplementalHandlers)

	if handlerCount > 0 {
		rootHandler.SetNext(supplementalHandlers[0])
	}

	for idx, handler := range supplementalHandlers {
		if idx == handlerCount-1 {
			break
		}

		handler.SetNext(supplementalHandlers[idx+1])
	}

	return &MiddlewareChain{rootHandler}
}

type PassThrough struct {
	handler http.Handler
}

func (m *PassThrough) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	m.handler.ServeHTTP(w, r)
}

func (*PassThrough) SetNext(http.Handler) {
	log.Fatal("SetNext called on PassThrough, this struct does not support this method")
}

type AllowedMethodFilter struct {
	methods []string
	next    http.Handler
}

func NewAllowedMethodFilter(methods []string) *AllowedMethodFilter {
	return &AllowedMethodFilter{methods, nil}
}

func (m *AllowedMethodFilter) SetNext(h http.Handler) {
	m.next = h
}

func (m *AllowedMethodFilter) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	allowed := false

	for _, method := range m.methods {
		if r.Method == method {
			allowed = true
			break
		}
	}

	if !allowed {
		http.Error(w, "Invalid HTTP method presented to endpoint", http.StatusMethodNotAllowed)
	}

	if m.next != nil {
		m.next.ServeHTTP(w, r)
	}
}

type AuthenticationFilter struct {
	config AuthConfig
	next   http.Handler
}

func NewAuthenticationFilter(config AuthConfig) *AuthenticationFilter {
	return &AuthenticationFilter{config, nil}
}

func (m *AuthenticationFilter) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	sessionCookie, err := r.Cookie("id")
	var sessionID string

	// TODO(garrett): Evaluate separate middleware for enforcing client tracking instead of
	// having it directly in the authentication logic
	// TODO(garrett): Include expiry as a check here
	if err == http.ErrNoCookie || !m.config.Session.Exists(sessionCookie.Value) {
		// TODO(garrett): Inlcude non-sensitive request info
		// TODO(garrett): Migrate to logging subsystem
		log.Println("Initiated anonymous session creation")
		sessionID, err = m.config.Session.Create()

		if err != nil {
			http.Error(w, "Unable to create new anonymous session", http.StatusInternalServerError)
			return
		}

		// TODO(garrett): Evaluate additional session cookie flags
		// NOTE(garrett): Likely should force secure but this makes local dev difficult.
		// We should explore options here, possibly setting secure only if we're listening
		// with TLS
		w.Header().Set("Set-Cookie", fmt.Sprintf("id=%s; HttpOnly; Path=/", sessionID))
	} else {
		sessionID = sessionCookie.Value
	}

	authenticated, err := m.config.Session.Authenticated(sessionID)

	if err != nil {
		http.Error(w, "Failed to check session authentication status", http.StatusInternalServerError)
		return
	}

	if !authenticated {
		m.config.AuthN.Authenticate(w, r, m.next)
	} else {
		if m.next != nil {
			m.next.ServeHTTP(w, r)
		}
	}
}

func (m *AuthenticationFilter) SetNext(h http.Handler) {
	m.next = h
}
