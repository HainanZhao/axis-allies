#!/usr/bin/env zsh
set -e
cd "$(dirname "$0")"
if [ ! -f "bin/axis_allies_2004" ]; then
    make -j12
fi
exec ./bin/axis_allies_2004 "$@"
