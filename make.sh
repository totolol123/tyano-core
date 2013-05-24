#!/bin/sh

set -e

if [ ! -f ".intermediate/Makefile" ]
then
	echo "Please run ./prepare.sh first!"
	exit
fi

echo "Compiling..."
echo "Note: You can pass arguments for 'make' to this script."
echo

mkdir -p .intermediate
cd .intermediate

echo "make $*"
make $*
