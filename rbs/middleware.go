package main

import (
	"net/http"
)

type AllowedMethodMiddleware struct {
	methods []string
	next    http.Handler
}

func NewAllowedMethodMiddleware(next http.Handler, method string, methods ...string) *AllowedMethodMiddleware {
	allowedMethods := make([]string, len(methods)+1)
	allowedMethods[0] = method

	for idx, additionalMethod := range methods {
		allowedMethods[idx+1] = additionalMethod
	}

	return &AllowedMethodMiddleware{
		methods: allowedMethods,
		next:    next,
	}
}

func (m *AllowedMethodMiddleware) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	allowed := false

	for _, method := range m.methods {
		if r.Method == method {
			allowed = true
			break
		}
	}

	if !allowed {
		http.Error(w, "Invalid HTTP method presented to endpoint", http.StatusMethodNotAllowed)
		return
	}

	m.next.ServeHTTP(w, r)
}
