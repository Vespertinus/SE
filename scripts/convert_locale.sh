#!/bin/bash

# $1 resource_root   $2 project_root

echo "convert locale tables to selt"
for json in "$1/locale/"*.json; do
    echo "  $json"
    flatc -b -o "$1/locale/" "$2/misc/UILocaleTable.fbs" "$json"
done
