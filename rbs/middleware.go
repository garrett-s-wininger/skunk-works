package main

import (
	"net/http"
)

type Middleware interface {
	http.Handler
	ShouldContinue() bool
}

type MiddlewareChain struct {
	handlers []Middleware
}

func (m *MiddlewareChain) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	for _, handler := range m.handlers {
		handler.ServeHTTP(w, r)

		if !handler.ShouldContinue() {
			break
		}
	}
}

type TerminalHandler struct {
	handler http.Handler
}

func (m *TerminalHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	m.handler.ServeHTTP(w, r)
}

func (*TerminalHandler) ShouldContinue() bool {
	return false
}

type AllowedMethods struct {
	errored bool
	methods []string
}

func NewAllowedMethods(method string, methods ...string) *AllowedMethods {
	// TODO(garrett): Validate that we're actually using HTTP methods and not random strings
	allowedMethods := make([]string, len(methods)+1)
	allowedMethods[0] = method

	for idx, additionalMethod := range methods {
		allowedMethods[idx+1] = additionalMethod
	}

	return &AllowedMethods{
		errored: false,
		methods: allowedMethods,
	}
}

func (m *AllowedMethods) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	allowed := false

	for _, method := range m.methods {
		if r.Method == method {
			allowed = true
			break
		}
	}

	if !allowed {
		http.Error(w, "Invalid HTTP method presented to endpoint", http.StatusMethodNotAllowed)
		m.errored = true
	}
}

func (m *AllowedMethods) ShouldContinue() bool {
	return !m.errored
}
