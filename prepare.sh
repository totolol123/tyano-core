#!/bin/sh

set -e

echo "Preparing project..."
echo "Note: You can pass arguments for './configure' to this script."
echo

mkdir -p .intermediate
cd .intermediate
ln -fs ../build/configure.ac
ln -fs ../build/Makefile.am
ln -fs ../build/m4
ln -fs ../sources
autoreconf -vi

echo "./configure -C $*"
./configure -C $*

echo "Done!"
echo "Continue with ./make.sh"
