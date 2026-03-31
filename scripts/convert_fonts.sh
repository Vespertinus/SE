#!/bin/bash

# $1 resource_root   $2 project_root

echo "convert font list to sefl"
flatc -b -o "$1/font/" "$2/misc/UIFontList.fbs" "$1/font/fonts.json"
