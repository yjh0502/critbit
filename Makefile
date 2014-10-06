CC=gcc
CXX=g++
FLAGS=-pthread -Wall -Werror -Os -g -ggdb
CFLAGS=$(FLAGS) -std=gnu99
CXXFLAGS=$(FLAGS) -std=c++0x

CXXSRCS= stlmap.cc stluomap.cc \
	sparsehash.cc
SRCS=none.c \
	bsdtree.c \
	uthash.c redisdict.c \
	art.c \
	critbit.c

export ITER=10000

OBJS=$(SRCS:.c=.o) $(CXXSRCS:.cc=.o)
BINS=$(OBJS:.o=.bin)
OUTS=$(OBJS:.o=.out)

export TIME=%E %M

.PHONY: bins run perfstat perf valgrind

perfstat: critbit.bin
	perf stat ./$<

perf: critbit.bin
	perf record ./$< && perf annotate ./$< && perf report

valgrind: critbit.bin
	valgrind ./$< 2>&1

bins: $(BINS)

all: run $(BINS)

run: $(OUTS)

%.out: %.bin
	time ./$< 2>&1 | tee $@

%.bin: %.o helper.o
	$(CXX) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.cc cc_common.h
	$(CXX) $(CXXFLAGS) -c $<

$(OBJS): helper.h critbit_common.h Makefile

clean:
	rm -f *.o *.bin *.out
