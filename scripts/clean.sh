#!/bin/bash
shopt -s extglob
cd "$(dirname $(dirname "$0"))/build"

# Delete all in build except berkeleydb
ls | grep -v "berkeleydb" | xargs rm -rf 
