#!/usr/bin/env bash

set -eu

if [ $# -ne 1 ]; then
	echo "Usage: benchmark.sh <json-and-bonjson-file-basename>"
        echo "You will need two files containing equivalent data: <file>.json and <file>.boj"
	exit 1
fi

SRC_FILE=$1

introduce-benchmark() {
        local type=$1
        local path=$2
        file_size_kb=`du -k "$path" | cut -f1`
        echo
        echo "Benchmarking $type with ${file_size_kb}k file $path"
}

benchmark-json() {
        local src_file=$1
        local dst_file=$2
        cat "$src_file" | jq -c > "$dst_file"
}

benchmark-bonjson() {
        local src_file=$1
        local dst_file=$2
        cat "$src_file" | ./build/bonjson-benchmark > "$dst_file"
}

benchmark-bonjson-decode() {
        local src_file=$1
        cat "$src_file" | ./build/bonjson-benchmark -d
}

BONJSON_TMPFILE=$(mktemp /tmp/bonjson-benchmark.XXXXXX.boj)
JSON_TMPFILE=$(mktemp /tmp/json-benchmark.XXXXXX.json)

rm -f "$BONJSON_TMPFILE"
rm -f "$JSON_TMPFILE"

introduce-benchmark "BONJSON (decode+encode)" "${SRC_FILE}.boj"
time benchmark-bonjson "${SRC_FILE}.boj" "$BONJSON_TMPFILE"
# ls -l "$BONJSON_TMPFILE"

introduce-benchmark "BONJSON (decode only)" "${SRC_FILE}.boj"
time benchmark-bonjson-decode "${SRC_FILE}.boj"

introduce-benchmark "JSON (decode+encode)" "${SRC_FILE}.json"
time benchmark-json "${SRC_FILE}.json" "$JSON_TMPFILE"
# ls -l "$JSON_TMPFILE"

rm -f "$BONJSON_TMPFILE"
rm -f "$JSON_TMPFILE"

echo
