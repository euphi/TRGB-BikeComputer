#!/bin/sh
# Install or update the bike log service outside the repo checkout.
#
#   Tools/BikeLogService/install.sh [PREFIX]        (default: ~/bikelog)
#
# PREFIX/venv          own venv; the code is installed into it, not linked, so
#                      switching branches in the repo does not touch the service
# PREFIX/data          sessions + SQLite index
# PREFIX/bikelog.env   configuration, created once, never overwritten
# PREFIX/bikelog.service  systemd unit (system unit with User=, see the file)
#
# Re-run after pulling new code to update; then: sudo systemctl restart bikelog
set -eu

TOOLS=$(cd "$(dirname "$0")/.." && pwd)
PREFIX=${1:-$HOME/bikelog}
USER_NAME=$(id -un)

mkdir -p "$PREFIX/data"
[ -x "$PREFIX/venv/bin/python" ] || python3 -m venv "$PREFIX/venv"
"$PREFIX/venv/bin/pip" install --quiet --upgrade pip
# Build from a copy: an in-tree build would leave build/ and *.egg-info in the repo.
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT
cp -r "$TOOLS/pyproject.toml" "$TOOLS/bikelog" "$BUILD/"
mkdir -p "$BUILD/BikeLogService"
cp -r "$TOOLS/BikeLogService/bikelogservice" "$BUILD/BikeLogService/"
"$PREFIX/venv/bin/pip" install --quiet --upgrade "$BUILD[service]"

if [ ! -f "$PREFIX/bikelog.env" ]; then
    cat > "$PREFIX/bikelog.env" <<EOF
# BikeLog service configuration -- see Tools/BikeLogService/README.md
BIKELOG_PORT=8080
BIKELOG_DATA_DIR=$PREFIX/data
BIKELOG_PULL=1
# device=mdns-host, comma separated
BIKELOG_PULL_TARGETS=trgb=TRGB-BC
BIKELOG_PULL_INTERVAL_S=120
# BIKELOG_REQUIRE_AUTH=1
# BIKELOG_TOKENS=<token>:<name>
EOF
    echo "created $PREFIX/bikelog.env"
fi

sed -e "s|@PREFIX@|$PREFIX|g" -e "s|@USER@|$USER_NAME|g" \
    "$TOOLS/BikeLogService/bikelog.service" > "$PREFIX/bikelog.service"

echo "installed into $PREFIX ($("$PREFIX/venv/bin/pip" show bikelog | sed -n 's/^Version: //p'))"
if [ ! -e /etc/systemd/system/bikelog.service ]; then
    echo "to run it as a service (once, needs root):"
    echo "  sudo install -m 644 $PREFIX/bikelog.service /etc/systemd/system/"
    echo "  sudo systemctl daemon-reload && sudo systemctl enable --now bikelog"
else
    echo "restart to pick up the new code:  sudo systemctl restart bikelog"
    echo "(if bikelog.service itself changed: sudo install -m 644 $PREFIX/bikelog.service /etc/systemd/system/ && sudo systemctl daemon-reload)"
fi
