CC = gcc -lpthread
CFLAGS =  -Wall -pedantic -std=gnu99 -I/local/courses/csse2310/include
LIBDIR = /local/courses/csse2310/lib
LDFLAGS = -L$(LIBDIR) -lcsse2310a3 -L$(LIBDIR) -lcsse2310a4 -lm
DEBUG = -g
TARGETS = auctionclient auctioneer 

.DEFAULT_GOAL := all 
.PHONY: all debug clean
all: $(TARGETS)

debug: CFLAGS += $(DEBUG)
debug: clean all

auctionclient.o: auctionclient.c
	$(CC) $(CFLAGS) -c $^ -o $@

autionclient: auctionclient.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

auctioneer.o: auctioneer.c
	$(CC) $(CFLAGS) -c $^ -o $@

auctioneer: auctioneer.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f $(TARGETS) *.o
