#!/bin/bash
shopt -s extglob
cd "$(dirname $(dirname "$0"))/build"

# Delete all in build except berkeleydb
rm -rf !(berkeleydb) .[!.]* ..?*
