#!/bin/bash
set -e

VERSION=$1
[ -z $VERSION ] && echo "require a version; e.g. 0.3.2" && exit 1
git status | grep "Changes not staged for commit" && echo "require a clean working tree" && exit 1

# ensure pregenerated boot files are up to date
echo "--- make pregen"
make pregen
git add boot/pregen/*

echo "--- git add config.h"
# update version in config.h
sed -i .o -e "s/define TL_VERSION_RAW .*/define TL_VERSION_RAW $VERSION/g" config.h
git add config.h

echo "--- git commit and tag"
# create commit and tag
git commit -m"releasing $VERSION"
git tag -a $VERSION -m"$VERSION"

echo "--- make test"
# make sure it works
make clean
make BUILD=release test

echo "--- all good, pushing"
git push origin $VERSION HEAD

