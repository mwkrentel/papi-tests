#
#  PAPI tests to see what events work with overflow, check that
#  interrupts don't die, fork and exec work, etc.
#
#  Copyright (c) 2009-2013, Rice University.
#  See the file LICENSE for details.
#
#  Mark W. Krentel, Rice University
#  October 2009
#
#  Note: keep the compiler optimization levels fairly low.  We're
#  trying to trigger performance counter events, not make the program
#  run fast, and we don't want the compiler to optimize out code that
#  triggers events.
#
#  $Id: Makefile 282 2013-07-22 21:45:26Z krentel $
#

PAPI = /home/krentel/papi-tests/install
PAPI_INC = -I $(PAPI)/include
PAPI_LIB = $(PAPI)/lib/libpapi.a

# The option -mfpmath=sse is x86-64 specific.
CC = gcc
CFLAGS = -g -O -Wall 
LDFLAGS =

# Some non-gcc compilers (eg, xlc) have trouble with the asm lines in
# context.c, so use an alternate compiler (gcc), if needed.
GCC = $(CC)
GCCFLAGS = $(CFLAGS)

HEADER_FILES = papi-tests.h
UTIL_OBJS = cycles.o utils.o
PAPI_UTIL_OBJS = papi-utils.o

REG_PROGRAMS = context exec fork handler mult-events nonthread over-avail throttle
THR_PROGRAMS = threads thread-over
PAPI_PROGRAMS = $(REG_PROGRAMS) $(THR_PROGRAMS)
TIMER_PROGRAMS = itimer ctimer rtimer

PROGRAMS = $(PAPI_PROGRAMS) $(TIMER_PROGRAMS)

.PHONY: all papi timer clean distclean

all: $(PROGRAMS)
papi:  $(PAPI_PROGRAMS)
timer: $(TIMER_PROGRAMS)

$(UTIL_OBJS): $(HEADER_FILES)
$(PAPI_UTIL_OBJS): $(HEADER_FILES)
$(TIMER_PROGRAMS): $(UTIL_OBJS)
$(PAPI_PROGRAMS): $(UTIL_OBJS) $(PAPI_UTIL_OBJS)

%.o: %.c
	$(CC) -o $@ -c $(CFLAGS) $(PAPI_INC) $<

$(UTIL_OBJS): %.o: %.c
	$(CC) -o $@ -c $(CFLAGS) $<

$(REG_PROGRAMS): %: %.o
	$(CC) -o $@ $(LDFLAGS) $< $(UTIL_OBJS) $(PAPI_UTIL_OBJS) $(PAPI_LIB)

$(THR_PROGRAMS): %: %.o
	$(CC) -o $@ $(LDFLAGS) $< $(UTIL_OBJS) $(PAPI_UTIL_OBJS) $(PAPI_LIB) -lpthread

context.o: context.c
	$(GCC) -o $@ -c $(GCCFLAGS) $(PAPI_INC) $<

itimer: itimer.o
	$(CC) -o $@ $(LDFLAGS) $< $(UTIL_OBJS) -lpthread

ctimer: ctimer.o
	$(CC) -o $@ $(LDFLAGS) $< $(UTIL_OBJS) -lpthread -lrt

rtimer: rtimer.o
	$(CC) -o $@ $(LDFLAGS) $< $(UTIL_OBJS) -lpthread -lrt

itimer.o: itimer.c
	$(CC) -o $@ -c $(CFLAGS) -DITIMER $<

ctimer.o: itimer.c
	$(CC) -o $@ -c $(CFLAGS) -DCTIMER $<

rtimer.o: itimer.c
	$(CC) -o $@ -c $(CFLAGS) -DRTIMER $<

clean:
	rm -f *.o

distclean: clean
	rm -f *~ a.out $(PROGRAMS)

