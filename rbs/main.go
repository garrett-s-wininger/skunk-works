package main

import (
        "io"
        "log"
        "net/http"
)

func main() {
        // TODO(garrett): Proper multiplexing
        handler := func(w http.ResponseWriter, req *http.Request) {
                io.WriteString(w, "Responding!\n")
        }

        http.HandleFunc("/", handler)

        // TODO(garrett): Support command line flags for port selection
        log.Fatal(http.ListenAndServe(":8080", nil))
}
