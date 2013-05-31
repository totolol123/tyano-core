#!/bin/bash -e

if [[ ! -f ".intermediate/Makefile" ]]
then
	echo "Please run ./prepare.sh first!"
	exit -1
fi

if [[ -z "$*" ]]; then
	echo "Note: You can pass arguments for 'make' to this script."
fi

build/link-sources.sh

echo "Compiling..."

cd .intermediate

echo "make $*"
make $*
