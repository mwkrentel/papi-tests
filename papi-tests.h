/*
 *  Common PAPI tests header file.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  October 2009
 *
 *  $Id: papi-tests.h 281 2013-07-11 19:40:04Z krentel $
 *
 *  Note: don't #include <papi.h> in this file.  We want to use it for
 *  both PAPI and non-PAPI programs.  Instead, include <papi.h> before
 *  this file and test for that inside here.
 */

#ifndef _PAPI_TESTS_H_
#define _PAPI_TESTS_H_

#include <sys/time.h>

#define MAX_EVENTS   20
#define MAX_THREADS  550

#define DEFAULT_PROG_TIME	60
#define DEFAULT_NUM_THREADS	4
#define DEFAULT_THRESHOLD	2000000
#define DEFAULT_WORK		1000
#define DEFAULT_MEMSIZE		40
#define DEFAULT_HANDLER_ITER	50
#define DEFAULT_STAGGER_DELAY   0

struct prog_args {
    int prog_time;
    int num_threads;
    int overflow;
    int work;
    int memsize;
    int handler_iter;
    int manual_restart;
    int single;
    int stagger_delay;
    int sleep;
    int verbose;
    int num_events;
    char *name[MAX_EVENTS];
    int event[MAX_EVENTS];
    int threshold[MAX_EVENTS];
};

struct memory_state {
    int *addr;
    size_t bytes;
    long size;
    long seed;
};

struct min_max_report {
    long total;
    long num;
    long min;
    long max;
    float avg;
    int pass;
};

typedef void papi_handler_t(int, void *, long long, void *);

void init_memory(struct memory_state *, int);
int  run_memory(struct memory_state *, int);
int  run_flops(int);

void usage(char *);
void set_default_args(struct prog_args *);
int  parse_args(struct prog_args *, int, char **);
void get_papi_events(struct prog_args *, int, int, char **);
void print_event_list(struct prog_args *);
int  event_set_for_overflow(struct prog_args *, papi_handler_t *);

#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

#ifdef PAPI_VERSION
#define TOT_CYC_DEFAULT(args)		\
    (args).name[0] = "PAPI_TOT_CYC";	\
    (args).event[0] = PAPI_TOT_CYC;	\
    (args).threshold[0] = (args).overflow;  \
    (args).num_events = 1;
#endif

#define INIT_REPORT(rep)	\
    (rep).total = 0;		\
    (rep).num = 0;		\
    (rep).min = 500000000;	\
    (rep).max = 0;

#define ADD_TO_REPORT(rep, count)	 \
    (rep).total += (count);		 \
    (rep).num++;			 \
    (rep).min = MIN((rep).min, (count)); \
    (rep).max = MAX((rep).max, (count));

#define EXIT_PASS_FAIL(pass)  \
    if (pass) { printf("PASSED\n"); exit(0); }  \
    else { printf("FAILED\n"); exit(1); }

static inline float
time_sub(struct timeval b, struct timeval a)
{
    return (float)(b.tv_sec - a.tv_sec)
	   + ((float)(b.tv_usec - a.tv_usec))/1000000.0;
}

/*
 *  98 is a generator for the prime 10,000,019.  This implies that
 *  g^1, g^2, ..., g^{p-1} is a pseudo-random permutation of 1, 2,
 *  ..., p-1.
 */
#define PRIME  10000019L
#define GEN    98
 
static inline long
random_gen(long x)
{
    return (x * GEN) % PRIME;
}

#endif
