#!/bin/sh
# Builds certs/x509_crt_bundle.bin, the root CA bundle the log push (src/LogPush.cpp) checks HTTPS
# servers against -- ESP-IDF's "common CAs" list (Mozilla's store, filtered to the CAs that cover
# nearly all public servers, Let's Encrypt included). The Arduino libraries do not ship the bundle
# IDF builds into its own apps, so it is embedded via board_build.embed_files (platformio.ini).
# Run again after an ESP-IDF update to refresh it, and commit the result.
set -e
IDF="${IDF_PATH:-$HOME/.platformio/packages/framework-espidf}"
DIR="$IDF/components/mbedtls/esp_crt_bundle"
cd "$(dirname "$0")/../certs"
python3 "$DIR/gen_crt_bundle.py" --quiet --input "$DIR/cacrt_all.pem" --filter "$DIR/cmn_crt_authorities.csv"
ls -l x509_crt_bundle
mv x509_crt_bundle x509_crt_bundle.bin
