#!/bin/sh
# Copies the TrailBridge BLE protocol into doc/trailbridge/ for the documentation site.
# The protocol lives in the TrailBridge repo (source of truth): PROTOCOL.md is German,
# PROTOCOL.en.md English. Here X.md is English and X.de.md German (see mkdocs.yml).
# Uses ../TrailBridge if it exists, otherwise clones the public repo.
set -e
cd "$(dirname "$0")/.."
src=../TrailBridge
if [ ! -f "$src/PROTOCOL.md" ]; then
	src=$(mktemp -d)
	git clone --quiet --depth 1 https://github.com/euphi/TrailBridge.git "$src"
fi
cp "$src/PROTOCOL.en.md" doc/trailbridge/PROTOCOL.md
cp "$src/PROTOCOL.md" doc/trailbridge/PROTOCOL.de.md
echo "TrailBridge protocol copied from $src"
