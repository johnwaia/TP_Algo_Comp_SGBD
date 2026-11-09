CC     = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Icore -Iindex
LIB    = core/table.c core/util.c index/index_hash.c index/index_avl.c index/index_list.c
HDRS   = core/minidb.h index/index.h

all: demo test bench

demo: exemples/demo.c $(LIB) $(HDRS)
	$(CC) $(CFLAGS) exemples/demo.c $(LIB) -o demo

test: tests/test.c $(LIB) $(HDRS)
	$(CC) $(CFLAGS) tests/test.c $(LIB) -o test

bench: benchmark/bench.c $(LIB) $(HDRS)
	$(CC) $(CFLAGS) benchmark/bench.c $(LIB) -o bench

clean:
	rm -f demo test bench demo.exe test.exe bench.exe

.PHONY: all clean
