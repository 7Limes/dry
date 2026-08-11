#!/bin/bash
set -euo pipefail

SRC_DIR="stdlib"
OUT_DIR="src/stdlib"

mkdir -p "$OUT_DIR"

names=()

# Generate a header for each file in stdlib
for file in "$SRC_DIR"/*; do
    [ -f "$file" ] || continue
    filename="$(basename "$file")"
    name="${filename%.*}"
    (cat "$file"; printf '\0') | xxd -i -n "stdlib_$name" > "$OUT_DIR/$name.h"
    echo "Generated $OUT_DIR/$name.h from $file"
    names+=("$name")
done

# Generate src/stdlib/stdlib.c from the collected names
STDLIB_C="$OUT_DIR/stdlib.c"

{
    echo "#include \"../util/map.h\""
    echo
    for name in "${names[@]}"; do
        echo "#include \"$name.h\""
    done
    echo
    echo "Map STDLIB_LOOKUP = {0};"
    echo
    echo
    echo "void init_stdlib_lookup() {"
    echo "    Map *m = &STDLIB_LOOKUP;"
    echo "    map_create(m, 64);"
    echo
    for name in "${names[@]}"; do
        echo "    map_add(m, \".$name\", stdlib_$name);"
    done
    echo "}"
} > "$STDLIB_C"

echo "Generated $STDLIB_C"