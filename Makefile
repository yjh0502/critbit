CC=gcc
CXX=g++
FLAGS=-Wall -Werror -O2
CFLAGS=$(FLAGS) -std=c99
CXXFLAGS=$(FLAGS) -std=c++0x

CXXSRCS= stlmap.cc stluomap.cc \
	sparsehash.cc
SRCS=none.c \
	bsdtree.c \
	uthash.c redisdict.c \
	art.c critbit.c

export ITER=100000

OBJS=$(SRCS:.c=.o) $(CXXSRCS:.cc=.o)
BINS=$(OBJS:.o=.bin)
OUTS=$(OBJS:.o=.out)

export TIME=%E %M

.PHONY: bins run

bins: $(BINS)

all: run $(BINS)

run: $(OUTS)

%.out: %.bin
	time ./$< 2>&1 | tee $@

%.bin: %.o helper.o
	$(CXX) $(CFLAGS) $^ -o $@ && strip -s $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.cc cc_common.h
	$(CXX) $(CXXFLAGS) -c $<

$(OBJS): helper.h Makefile

clean:
	rm -f *.o *.bin *.out
