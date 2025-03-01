#!/usr/bin/env bash

set -eu

if [ $# -ne 1 ]; then
	echo "Usage: benchmark.sh <json-and-bonjson-file-basename>"
        echo "You will need two files containing equivalent data: <file>.json and <file>.bjn"
	exit 1
fi

SRC_FILE=$1

introduce-benchmark() {
        local type=$1
        local path=$2
        file_size_kb=`du -k "$path" | cut -f1`
        echo
        echo "Benchmarking $type decode+encode with ${file_size_kb}k file $path"
}

json-benchmark() {
        local src_file=$1
        local dst_file=$2
        introduce-benchmark "JSON" "$src_file"
        cat "$src_file" | jq > "$dst_file"
}

bonjson-benchmark() {
        local src_file=$1
        local dst_file=$2
        introduce-benchmark "BONJSON" "$src_file"
        cat "$src_file" | ./build/bonjson-benchmark > "$dst_file"
}

BONJSON_TMPFILE=$(mktemp /tmp/bonjson-benchmark.XXXXXX.bjn)
JSON_TMPFILE=$(mktemp /tmp/json-benchmark.XXXXXX.json)

rm -f "$BONJSON_TMPFILE"
rm -f "$JSON_TMPFILE"

time bonjson-benchmark "${SRC_FILE}.bjn" "$BONJSON_TMPFILE"
time json-benchmark "${SRC_FILE}.json" "$BONJSON_TMPFILE"

rm -f "$BONJSON_TMPFILE"
rm -f "$JSON_TMPFILE"
