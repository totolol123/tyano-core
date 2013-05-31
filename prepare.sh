#!/bin/bash -e

if [[ -z "$*" ]]; then
	echo "Note: You can pass arguments for './configure' to this script."
fi

echo "Preparing project..."

mkdir -p .intermediate

chmod u=rwx,g=,o= *.sh
chmod u=rwx,g=,o= build/link-sources.sh
build/link-sources.sh

cd .intermediate
ln -fs ../build/configure.ac
ln -fs ../build/Makefile.am
ln -fs ../build/m4

echo "Generating configure..."
autoreconf -i

echo "./configure -C $*"
./configure -C $*

echo "Done!"
echo "Continue with ./make.sh"
