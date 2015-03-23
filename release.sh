#!/bin/bash
set -e
VERSION=$1
[ -z $VERSION ] && echo "require a version; e.g. 0.3.2" && exit 1
git status | grep "Changes not staged for commit" && echo "require a clean working tree" && exit 1

sed -i -e "s/define TL_VERSION_RAW .*/define TL_VERSION_RAW $VERSION/g" config.h
git add config.h
git commit -m"releasing $VERSION"
git tag -a $VERSION -m"$VERSION"
make clean
make BUILD=release test

echo "all good, pushing"
git push origin $VERSION
git push

