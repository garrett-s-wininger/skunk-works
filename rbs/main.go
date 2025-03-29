package main

import (
	"html/template"
	"log"
	"net/http"
	"path/filepath"
)

func index(w http.ResponseWriter, req *http.Request) {
	paths := []string{
		filepath.Join("templates/index.tmpl"),
	}

	templ := template.Must(template.ParseFiles(paths...))
	err := templ.Execute(w, nil)

	if err != nil {
		log.Fatal("Template execution failure: %s", err)
	}
}

func main() {
	mux := http.NewServeMux()

	mux.HandleFunc("/{$}", index)
	mux.Handle(
		"/static/",
		http.StripPrefix("/static/", http.FileServer(http.Dir("./static"))))

	// TODO(garrett): Support command line flags for port selection
	log.Fatal(http.ListenAndServe(":8080", mux))
}
