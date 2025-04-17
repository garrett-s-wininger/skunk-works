package oidc

import (
	"testing"
)

func TestJOSEDecoding(t *testing.T) {
	encodedHeader := "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9"
	header, err := DecodeJOSEHeader(encodedHeader)

	if err != nil {
		t.Errorf("Failed to decode header")
	}

	if header.Algorithm != AlgorithmRS256 {
		t.Errorf("Expected RS256, received: %s", header.Algorithm)
	}

	if header.ID != "" {
		t.Errorf("Unexpected key ID decoded")
	}
}
