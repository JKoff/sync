UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
	LD=-ldl
endif
ifeq ($(UNAME), Darwin)
	LD=-framework CoreFoundation -framework CoreServices
endif

C = clang
CFLAGS ?= -O0 -g
CFLAGS += -I. -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes

CC=clang++
CCFLAGS=-std=c++11 -Wall -Werror -pedantic -g -pthread -Qunused-arguments
LDLIBS=libsnappy.a libfswatch.a libcrypto.a libssl.a
SNAPPY_OBJS=lib/snappy/snappy.o lib/snappy/snappy-c.o \
	lib/snappy/snappy-sinksource.o lib/snappy/snappy-stubs-internal.o

SYNC_C_SRCS=lib/retter/algorithms/xxHash/xxhash.c

SYNC_PRIMARY_SRCS=$(wildcard src/sync-primary.cpp src/index.cpp src/util.cpp src/*/*.cpp src/*/*/*.cpp)
SYNC_PRIMARY_OBJS=$(subst .c,.o,$(SYNC_C_SRCS)) $(subst .cpp,.o,$(SYNC_PRIMARY_SRCS))

SYNC_REPLICA_SRCS=$(wildcard src/sync-replica.cpp src/index.cpp src/util.cpp src/*/*.cpp src/*/*/*.cpp)
SYNC_REPLICA_OBJS=$(subst .c,.o,$(SYNC_C_SRCS)) $(subst .cpp,.o,$(SYNC_REPLICA_SRCS))

SYNC_CTL_SRCS=$(wildcard src/sync-ctl.cpp \
	src/net/unix-client.cpp src/net/socket.cpp src/net/protocol.cpp \
	src/fs/types.cpp \
	src/util.cpp src/util/*.cpp)
SYNC_CTL_OBJS=$(subst .c,.o,$(SYNC_C_SRCS)) $(subst .cpp,.o,$(SYNC_CTL_SRCS))

all: $(LIBNAME) sync-primary sync-replica sync-ctl

libcrypto.a:
	cd lib/openssl; export KERNEL_BITS=64; ./config; make; cd ../..
	cp lib/openssl/libcrypto.a .
	cp lib/openssl/libssl.a .

libsnappy.a: $(SNAPPY_OBJS)
	ar rcs $@ $^

libfswatch.a:
	make -C lib/fswatch/libfswatch
	ar rcs $@ lib/fswatch/libfswatch/src/libfswatch/c/*.o lib/fswatch/libfswatch/src/libfswatch/c++/*.o
	cp lib/fswatch/libfswatch/src/libfswatch/c/*.h lib/fswatch/libfswatch/src/libfswatch/c++/

lib/retter/algorithms/xxHash/xxhash.o:
	$(C) -c -o lib/retter/algorithms/xxHash/xxhash.o lib/retter/algorithms/xxHash/xxhash.c $(CFLAGS) -Iincludes

%.o: %.c
	$(CC) -c -o $(subst .c,.o,$<) $< $(CCFLAGS) -Iincludes

%.o: %.cpp
	$(CC) -c -o $(subst .cpp,.o,$<) $< $(CCFLAGS) -Iincludes

sync-primary: $(SYNC_PRIMARY_OBJS) $(LDLIBS)
	$(CC) $(CCFLAGS) -o sync-primary $(SYNC_PRIMARY_OBJS) $(LDLIBS) $(LD)

sync-replica: $(SYNC_REPLICA_OBJS) $(LDLIBS)
	$(CC) $(CCFLAGS) -o sync-replica $(SYNC_REPLICA_OBJS) $(LDLIBS) $(LD)

sync-ctl: $(SYNC_CTL_OBJS)
	$(CC) $(CCFLAGS) -o sync-ctl $(SYNC_CTL_OBJS) $(LDLIBS) $(LD)

clean: clean-src
	rm -f *.a
	rm -f *.o */*.o */*/*.o */*/*/*.o
	make -C lib/snappy clean
	make -C lib/fswatch/libfswatch clean
	make -C lib/openssl clean

clean-src:
	rm -f $(SYNC_PRIMARY_OBJS) $(SYNC_REPLICA_OBJS) $(SYNC_CTL_OBJS)
	rm -f sync-primary sync-replica sync-ctl
