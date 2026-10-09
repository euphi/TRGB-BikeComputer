#!/bin/sh
# Print the path of the archived firmware.elf for an ELF id (as shown on /debug/coredump),
# unpacked into /tmp if needed. Usage: Tools/fwarchive.sh 4f1cb0873   (no argument: list builds)
cd "$(dirname "$0")/../firmware-archive/builds" || exit 1
if [ -z "$1" ]; then
    for d in */; do printf '%s  ' "${d%/}"; python3 -c 'import json,sys;m=json.load(open(sys.argv[1]));print(m["built"],m["describe"],m["env"],"dirty" if m["dirty"] else "",m["tag"])' "$d/meta.json"; done | sort -k2
    exit
fi
d=$(ls -d "$1"* 2>/dev/null | head -1) || exit 1
[ -n "$d" ] || { echo "no build $1" >&2; exit 1; }
gunzip -ck "$d/firmware.elf.gz" > "/tmp/firmware-$d.elf" && echo "/tmp/firmware-$d.elf"
