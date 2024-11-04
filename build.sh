#!/bin/bash

# Build script for macOS on arm64.
# Dynamically links to libssl and libsnappy installed via Homebrew.

set -e

mkdir -p lib

cd lib

if [ ! -h retter ]; then
	wget "https://github.com/maciejczyzewski/retter/archive/master.zip"
	unzip master.zip
	ln -s retter-master retter
	rm master.zip
fi

cd ..

# if [ ! -f lib/fswatch/libfswatch/Makefile ]; then
# 	(cd lib/fswatch; exec ./configure)
# fi

# if [ ! -f lib/fswatch/libfswatch/Makefile ]; then
# 	(cd lib/fswatch/libfswatch; exec make)
# fi

export CCFLAGS="-Iincludes -I/opt/homebrew/opt/openssl@3/include -I/opt/homebrew/opt/snappy/include "
export LDFLAGS="-L/opt/homebrew/opt/openssl@3/lib -L/opt/homebrew/opt/snappy/lib"
export LDLIBS="/opt/homebrew/opt/openssl@3/lib/*.a /opt/homebrew/opt/snappy/lib/*.a"
make
