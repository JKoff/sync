#!/bin/bash

mkdir -p lib

cd lib

# Snappy

if [ ! -h snappy ]; then
	wget "https://github.com/google/snappy/releases/download/1.1.3/snappy-1.1.3.tar.gz"
	tar xvf snappy-1.1.3.tar.gz
	rm -f snappy
	ln -s snappy-1.1.3 snappy
	rm snappy-1.1.3.tar.gz
fi


# fswatch

if [ ! -h fswatch ]; then
	wget "https://github.com/emcrisostomo/fswatch/releases/download/1.4.7/fswatch-1.4.7.tar.gz"
	tar xvf fswatch-1.4.7.tar.gz
	ln -s fswatch-1.4.7 fswatch
	rm fswatch-1.4.7.tar.gz
fi


# retter

if [ ! -h retter ]; then
	wget "https://github.com/maciejczyzewski/retter/archive/master.zip"
	unzip master.zip
	ln -s retter-master retter
	rm master.zip
fi


# openssl

if [ ! -h openssl ]; then
	wget "https://www.openssl.org/source/openssl-1.0.2d.tar.gz"
	tar xvf openssl-1.0.2d.tar.gz
	ln -s openssl-1.0.2d openssl
	rm openssl-1.0.2d.tar.gz
fi


cd ..

if [ ! -f lib/snappy/Makefile ]; then
	(cd lib/snappy; exec ./configure)
fi

if [ ! -f lib/fswatch/libfswatch/Makefile ]; then
	(cd lib/fswatch/libfswatch; exec ./configure)
fi

if [ ! -f lib/openssl/Makefile ]; then
	(cd lib/fswatch/libfswatch; exec ./configure)
fi

make
