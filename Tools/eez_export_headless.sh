#!/bin/bash
# Headless EEZ Studio export of EEZStudio/TRGB-BikeComputer.eez-project into a scratch
# copy, then copy only the changed generated code/image files into src/ui_eez/.
# Why not export in place: the headless build (--build-project) does not regenerate
# ui_font_*.c, so its .eez-project-build list would drop them. Output files it does
# produce were verified byte-identical to a Ctrl+B export (2026-09-27).
# The user's own Ctrl+B in EEZ Studio remains the reference export.
set -e
REPO=$(cd "$(dirname "$0")/.." && pwd)
APPIMAGE=${EEZ_APPIMAGE:-$HOME/Downloads/EEZ-Studio-0.29.0.AppImage}
WORK=$(mktemp -d)
mkdir -p "$WORK/EEZStudio" "$WORK/src/ui_eez"
cp "$REPO/EEZStudio/TRGB-BikeComputer.eez-project" "$WORK/EEZStudio/"
cp -r "$REPO/EEZStudio/assets" "$WORK/EEZStudio/"
(cd "$WORK" && env -u ELECTRON_RUN_AS_NODE -u ELECTRON_NO_ATTACH_CONSOLE timeout 150 \
	"$APPIMAGE" --build-project "$WORK/EEZStudio/TRGB-BikeComputer.eez-project" 2>&1 \
	| grep -E "error|warning|Build successfully|Build failed" || true)
for f in "$WORK"/src/ui_eez/*; do
	b=$(basename "$f")
	case "$b" in ui_font_*|.eez-project-build) continue;; esac
	if [ ! -f "$REPO/src/ui_eez/$b" ]; then echo "NEW $b"; cp "$f" "$REPO/src/ui_eez/$b";
	elif ! cmp -s "$f" "$REPO/src/ui_eez/$b"; then echo "CHG $b"; cp "$f" "$REPO/src/ui_eez/$b"; fi
done
rm -rf "$WORK"
