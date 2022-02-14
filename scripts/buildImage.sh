#!/bin/bash
cd "$(dirname $(dirname "$0"))"

docker build -t benchmark:builder --target builder . # tag intermediate build so we don't loose it on prune
docker build -t benchmark .
