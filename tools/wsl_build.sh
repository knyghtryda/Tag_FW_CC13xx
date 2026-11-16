#!/bin/bash
# Build oepl_CC1310 in WSL from a copy on the Linux filesystem.
#
#   wsl -d Ubuntu -- bash tools/wsl_build.sh [CONFIG] [make args...]
#
# /mnt/c is case-insensitive, so "Board.h" (oepl_CC1310) would resolve to
# common/board.h when included from common/. Building from a copy under
# $HOME keeps upstream's case-sensitive include behaviour. The build output
# (build/<CONFIG>/oepl_CC1310.out and .map) is copied back into the repo.
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$HOME/build/Tag_FW_CC13xx"
CONFIG="${1:-Debug}"
shift || true

mkdir -p "$WORK"
rsync -a --delete --exclude .git --exclude dumps --exclude 'oepl_CC1310/build' \
      "$REPO/" "$WORK/"
make -C "$WORK/oepl_CC1310" CONFIG="$CONFIG" "$@"
mkdir -p "$REPO/oepl_CC1310/build"
rsync -a "$WORK/oepl_CC1310/build/$CONFIG/" "$REPO/oepl_CC1310/build/$CONFIG/"
