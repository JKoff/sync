UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
	LD=-ldl
endif
ifeq ($(UNAME), Darwin)
	LD=-framework CoreFoundation -framework CoreServices
endif

C ?= clang
CFLAGS += -O0 -g
CFLAGS += -I. -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes

CC ?= clang++
CCFLAGS += -std=c++17 -Wall -Werror -pedantic -g -pthread -Qunused-arguments

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

all: sync-primary sync-replica sync-ctl

%.o: %.c
	$(C) -c -o $(subst .c,.o,$<) $< $(CFLAGS)

%.o: %.cpp
	$(CC) -c -o $(subst .cpp,.o,$<) $< $(CCFLAGS) $(LDFLAGS)

sync-primary: $(SYNC_PRIMARY_OBJS)
	$(CC) $(CCFLAGS) -o sync-primary $(SYNC_PRIMARY_OBJS) $(LDFLAGS) $(LD) $(LDLIBS)

sync-replica: $(SYNC_REPLICA_OBJS)
	$(CC) $(CCFLAGS) -o sync-replica $(SYNC_REPLICA_OBJS) $(LDFLAGS) $(LD) $(LDLIBS)

sync-ctl: $(SYNC_CTL_OBJS)
	$(CC) $(CCFLAGS) -o sync-ctl $(SYNC_CTL_OBJS) $(LDFLAGS) $(LD) $(LDLIBS)

clean: clean-src
	rm -f *.o */*.o */*/*.o */*/*/*.o

clean-src:
	rm -f $(SYNC_PRIMARY_OBJS) $(SYNC_REPLICA_OBJS) $(SYNC_CTL_OBJS)
	rm -f sync-primary sync-replica sync-ctl
