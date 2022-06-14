/*
 *  Stress test for threaded PAPI interrupts.
 *
 *  This program should be able to run for a long time (hours) at a
 *  high interrupt rate (10,000/sec) with a pretty steady rate of
 *  interrupts per second.  The main danger is that interrupts may
 *  just stop and never resume.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  October 2009
 *
 *  $Id: threads.c 281 2013-07-11 19:40:04Z krentel $
 */

#include <sys/time.h>
#include <sys/types.h>
#include <err.h>
#include <error.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <papi.h>
#include "papi-tests.h"

static struct prog_args args;
static pthread_key_t key;

static int EventSet[MAX_THREADS];
static struct memory_state memstate[MAX_THREADS];
static struct min_max_report rep[MAX_THREADS];

static volatile long count[MAX_THREADS];
static volatile int ready[MAX_THREADS];
static volatile int done = 0;

void
my_handler(int EventSet, void *pc, long long ovec, void *context)
{
    int tid = *(int *)pthread_getspecific(key);

    if (tid < 0 || tid >= args.num_threads) {
	warnx("thread id from getspecific out of bounds: %d", tid);
	return;
    }
    count[tid]++;
}

/*
 *  Compute min, max, avg number of interrupts per segment of work
 *  (roughly 1-2 sec).  Declare SUCCESS if min and max are within 50%
 *  of average, and interrupts don't just up and disappear.
 */
void
run_test(int tid)
{
    struct timeval start, now, last;
    char *eol = (args.verbose) ? ", " : "\n";
    int work, num_errs;
    int my_start = 0;

    INIT_REPORT(rep[tid]);

    gettimeofday(&start, NULL);
    last = start;

    if (args.stagger_delay == 0) {
	if (PAPI_start(EventSet[tid]) != PAPI_OK) {
	    errx(1, "PAPI_start failed");
	}
	my_start = 1;
    }

    num_errs = 0;
    memstate[tid].seed = 1;
    do {
	if (! my_start) {
	    gettimeofday(&now, NULL);
	    if (time_sub(now, start) >= args.stagger_delay * tid) {
		printf("===> starting timer in thread %d\n", tid);
		if (PAPI_start(EventSet[tid]) != PAPI_OK) {
		    errx(1, "PAPI_start failed");
		}
		my_start = 1;
	    }
	}

	count[tid] = 0;
	work = 0;
	do {
	    num_errs += run_flops(5);
	    work += 5;
	    if (args.memsize > 0) {
		num_errs += run_memory(&memstate[tid], 5);
		work += 5;
	    }
	}
	while (work < args.work);

	gettimeofday(&now, NULL);
	if (tid == 0 || !args.single) {
	    printf("time: %.1f, tid: %d, work: %d, count: %ld%s",
		   time_sub(now, start), tid, work, count[tid], eol);
	    if (args.verbose) {
		float fcount = (float) count[tid];
		float fwork = (float) work;
		float delta_t = time_sub(now, last);
		printf("intr/sec: %.2f, intr/Kwork: %.2f, work/sec: %.2f\n",
		       fcount/delta_t, 1000.0*fcount/fwork, fwork/delta_t);
	    }
	}
	last = now;

	if (time_sub(now, start) > 5.0 && !done) {
	    ADD_TO_REPORT(rep[tid], count[tid]);
	}
    }
    while (time_sub(now, start) <= args.prog_time);

    PAPI_stop(EventSet[tid], NULL);

    /*
     * The memory accesses in one thread randomly distort the running
     * time of the other threads.  So, we need a looser criteria for
     * success.
     */
    rep[tid].avg = rep[tid].total / (float)rep[tid].num;
    rep[tid].pass = (num_errs == 0) && (rep[tid].min > 0.35 * rep[tid].avg)
	&& (rep[tid].max < 1.50 * rep[tid].avg);
}

void *
my_thread(void *data)
{
    int tid = *(int *)data;
    int k, num_ready;

    if (pthread_setspecific(key, data) != 0)
	errx(1, "pthread_setspecific failed");

    EventSet[tid] = event_set_for_overflow(&args, &my_handler);
    init_memory(&memstate[tid], args.memsize);

    /* Wait for all threads to finish init_memory. */
    ready[tid] = 1;
    do {
	num_ready = 0;
	for (k = 0; k < args.num_threads; k++)
	    num_ready += ready[k];
    }
    while (num_ready < args.num_threads);

    run_test(tid);
    done = 1;

    return NULL;
}

int
main(int argc, char **argv)
{
    pthread_t td[MAX_THREADS];
    int tid[MAX_THREADS];
    int k, opt, pass;

    set_default_args(&args);
    args.num_threads = DEFAULT_NUM_THREADS;
    opt = parse_args(&args, argc, argv);
    get_papi_events(&args, opt, argc, argv);
    if (args.num_events == 0) {
	TOT_CYC_DEFAULT(args);
    }
    args.prog_time = MAX(args.prog_time, 15);

    printf("Threads Stress test, time: %d, threads: %d\n",
	   args.prog_time, args.num_threads);
    print_event_list(&args);

    for (k = 0; k < MAX_THREADS; k++) {
	ready[k] = 0;
	tid[k] = k;
    }

    if (PAPI_thread_init(pthread_self) != PAPI_OK)
        errx(1, "PAPI_thread_init failed");

    if (pthread_key_create(&key, NULL) != 0)
        errx(1, "pthread key create failed");

    for (k = 1; k < args.num_threads; k++) {
	if (pthread_create(&td[k], NULL, my_thread, &tid[k]) != 0)
	    errx(1, "pthread create failed");
    }
    my_thread(&tid[0]);

    for (k = 1; k < args.num_threads; k++) {
	pthread_join(td[k], NULL);
    }

    printf("\nThreads Stress test, time: %d, threads: %d\n",
	   args.prog_time, args.num_threads);
    print_event_list(&args);

    pass = 1;
    for (k = 0; k < args.num_threads; k++) {
	printf("tid: %d, min: %ld, avg: %.1f, max: %ld\n",
	       k, rep[k].min, rep[k].avg, rep[k].max);
	pass = pass && rep[k].pass;
    }

    EXIT_PASS_FAIL(pass);
}
