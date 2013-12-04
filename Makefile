CC=gcc
CXX=g++
FLAGS=-pthread -Wall -Werror -O2 -g -ggdb
CFLAGS=$(FLAGS) -std=gnu99
CXXFLAGS=$(FLAGS) -std=c++0x

CXXSRCS= stlmap.cc stluomap.cc \
	sparsehash.cc
SRCS=none.c \
	bsdtree.c \
	uthash.c redisdict.c \
	art.c \
	critbit.c critbit_cow_stack.c

#export LD_PRELOAD=/usr/lib/libjemalloc.so.1
export ITER=10000

OBJS=$(SRCS:.c=.o) $(CXXSRCS:.cc=.o)
BINS=$(OBJS:.o=.bin)
OUTS=$(OBJS:.o=.out)

export TIME=%E %M

.PHONY: bins run perf valgrind

perf: critbit.bin
	perf stat ./$<
	#perf record ./$< && perf annotate ./$< && perf report

valgrind: critbit_cow_stack.bin
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
