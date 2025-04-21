package main

import (
	"html/template"
	"log"
	"net/http"
	"path/filepath"
)

func getApplicationRoutes(auth AuthConfig) *http.ServeMux {
	mux := http.NewServeMux()

	indexChain := NewMiddlewareChain(
		NewAllowedMethodFilter([]string{http.MethodGet, http.MethodHead}),
		NewAuthenticationFilter(auth),
		&PassThrough{http.HandlerFunc(index)},
	)

	mux.Handle("/{$}", indexChain)

	mux.Handle(
		"/static/",
		http.StripPrefix("/static/", http.FileServer(http.Dir("./static"))))

	auth.AuthN.RegisterHandlers(mux)
	return mux
}

func index(w http.ResponseWriter, r *http.Request) {
	paths := []string{
		filepath.Join("templates/index.tmpl"),
	}

	// NOTE(garrett): It's ideal if we take this hit once on startup and not on
	// every page request, less important in an SPA model. In a development mode,
	// we can either do the reload, or simply do a file watch. For prod though,
	// perhaps we should have system integrity checks to prevent tampering.
	templ := template.Must(template.ParseFiles(paths...))
	err := templ.Execute(w, nil)

	if err != nil {
		log.Fatal("Template execution failure: %s", err)
	}
}
