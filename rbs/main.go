package main

import (
        "html/template"
        "log"
        "net/http"
        "path/filepath"
)

func main() {
        paths := []string{
                filepath.Join("templates/index.tmpl"),
        }

        templ := template.Must(template.ParseFiles(paths...))

        // TODO(garrett): Proper multiplexing
        handler := func(w http.ResponseWriter, req *http.Request) {
                err := templ.Execute(w, nil)

                if err != nil {
                        log.Fatal("Template execution failure: %s", err)
                }
        }

        http.HandleFunc("/", handler)

        // TODO(garrett): Support command line flags for port selection
        log.Fatal(http.ListenAndServe(":8080", nil))
}
