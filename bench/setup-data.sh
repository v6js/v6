#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -W 2>/dev/null || pwd)"
DATA_DIR="$ROOT_DIR/bench/data"

SMALL_FILES_DIR="$DATA_DIR/small-files"
if [ ! -d "$SMALL_FILES_DIR" ] || [ -z "$(ls -A "$SMALL_FILES_DIR" 2>/dev/null)" ]; then
  echo "==> Generating $SMALL_FILES_DIR (5000 small files)"
  mkdir -p "$SMALL_FILES_DIR"
  i=0
  while [ "$i" -lt 5000 ]; do
    printf 'line one of file %d\nline two with some more content to read %d\n' "$i" "$i" \
      > "$SMALL_FILES_DIR/file$(printf '%05d' "$i").txt"
    i=$((i + 1))
  done
fi

LARGE_FILE="$DATA_DIR/large-file.txt"
if [ ! -f "$LARGE_FILE" ]; then
  echo "==> Generating $LARGE_FILE (~20MB)"
  : > "$LARGE_FILE"
  line="the quick brown fox jumps over the lazy dog 0123456789 abcdefghijklmnopqrstuvwxyz"
  block="$DATA_DIR/.line-block.tmp"
  : > "$block"
  i=0
  while [ "$i" -lt 2000 ]; do
    printf '%s\n' "$line" >> "$block"
    i=$((i + 1))
  done
  i=0
  while [ "$i" -lt 128 ]; do
    cat "$block" >> "$LARGE_FILE"
    i=$((i + 1))
  done
  rm -f "$block"
fi

MANY_DIRS="$DATA_DIR/many-dirs"
if [ ! -d "$MANY_DIRS" ] || [ -z "$(ls -A "$MANY_DIRS" 2>/dev/null)" ]; then
  echo "==> Generating $MANY_DIRS (nested tree)"
  mkdir -p "$MANY_DIRS"
  a=0
  while [ "$a" -lt 10 ]; do
    b=0
    while [ "$b" -lt 10 ]; do
      leaf="$MANY_DIRS/d$a/d$b"
      mkdir -p "$leaf"
      f=0
      while [ "$f" -lt 5 ]; do
        printf 'leaf file %d-%d-%d\n' "$a" "$b" "$f" > "$leaf/f$f.txt"
        f=$((f + 1))
      done
      b=$((b + 1))
    done
    a=$((a + 1))
  done
fi

SINGLE_DIR="$DATA_DIR/single-dir"
if [ ! -d "$SINGLE_DIR" ] || [ -z "$(ls -A "$SINGLE_DIR" 2>/dev/null)" ]; then
  echo "==> Generating $SINGLE_DIR (2000 files, one flat dir)"
  mkdir -p "$SINGLE_DIR"
  i=0
  while [ "$i" -lt 2000 ]; do
    : > "$SINGLE_DIR/f$(printf '%05d' "$i").txt"
    i=$((i + 1))
  done
fi

echo "Data setup complete."
