#!/bin/sh
# Cross-builds bin/Ricasso.exe on Linux/macOS. Requires Go 1.22+.
set -e
cd "$(dirname "$0")"
CGO_ENABLED=0 GOFLAGS=-mod=vendor go test ./importer
GOOS=windows GOARCH=amd64 CGO_ENABLED=0 GOFLAGS=-mod=vendor \
    go build -trimpath -ldflags "-s -w -H windowsgui" -o bin/Ricasso.exe .
echo "Starter: $(pwd)/bin/Ricasso.exe"
