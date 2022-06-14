/*
 *  PAPI threaded overhead test.
 *
 *  This program measures the amount of overhead from PAPI interrupts
 *  in threaded programs.  This is mostly intended for highly threaded
 *  systems such as IBM Blue Gene and Intel MIC as a way to estimate
 *  interrupt rates and the effect of scaling to more threads.
 *
 *  Overhead is the percentage loss in the amount of work per second
 *  compared to the rate with no interrupts.  In the threaded case, we
 *  count the total work summed over all threads.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  July 2013
 *
 *  $Id: thread-over.c 282 2013-07-22 21:45:26Z krentel $
 */

#include <sys/time.h>
#include <sys/types.h>
#include <err.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <papi.h>
#include "papi-tests.h"

#define DEFAULT_TIME   40
#define MIN_TIME       10
#define MAX_OVER_RATE  95.0
#define SIZE  25

enum { NONE = 0, INIT, RUN, STOP, EXIT };

static long Threshold[SIZE] = {
           0,  200000000,  100000000,
    50000000,   20000000,   10000000,
     5000000,    2000000,    1000000,
      500000,     200000,     100000,
       50000,      20000,         -1
};

static float Work[SIZE];
static float Intr[SIZE];
static float Overhead[SIZE];

static struct prog_args args;
static pthread_key_t key;

static int EventSet[MAX_THREADS];

volatile int state[MAX_THREADS];
volatile int ready[MAX_THREADS];

static long work[MAX_THREADS];
static long cur_work[MAX_THREADS];
static long prev_work[MAX_THREADS];
static long begin_work;
static long end_work;

static long count[MAX_THREADS];
static long cur_count[MAX_THREADS];
static long prev_count[MAX_THREADS];
static long begin_count;
static long end_count;

static struct timeval time_start;
static struct timeval time_begin;
static struct timeval time_end;

static float len_begin;
static float len_end;
static float fnum_threads;
static int   max_index;

static long  base_work = -1;
static float base_evrate = -1.0;

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

void
set_state(int st)
{
    int k;

    for (k = 0; k < args.num_threads; k++) {
	ready[k] = NONE;
	state[k] = st;
    }
}

void
wait_on_state(int st)
{
    int k, num_ready;

    do {
	num_ready = 1;
	for (k = 1; k < args.num_threads; k++) {
	    if (ready[k] == st) {
		num_ready++;
	    }
	}
    }
    while (num_ready < args.num_threads);
}

void
print_stats(struct timeval now, struct timeval last)
{
    long min_work, max_work, total_work, diff;
    long min_count, max_count, total_count;
    int k;

    for (k = 0; k < args.num_threads; k++) {
	cur_work[k] = work[k];
	cur_count[k] = count[k];
    }

    min_work = 500000000;
    max_work = 0;
    total_work = 0;
    min_count = 500000000;
    max_count = 0;
    total_count = 0;

    for (k = 0; k < args.num_threads; k++) {
	diff = cur_work[k] - prev_work[k];
	min_work = MIN(min_work, diff);
	max_work = MAX(max_work, diff);
	total_work += diff;
	diff = cur_count[k] - prev_count[k];
	min_count = MIN(min_count, diff);
	max_count = MAX(max_count, diff);
	total_count += diff;
    }

    printf("time: %.1f, work/thr: %ld %ld (%ld), "
	   "intr/thr: %ld %ld (%ld), evrate: %.4e\n",
	   time_sub(now, time_start),
	   min_work, max_work, total_work,
	   min_count, max_count, total_count,
	   ((float) (args.threshold[0] * total_count))/((float) total_work));

    for (k = 0; k < args.num_threads; k++) {
	prev_count[k] = cur_count[k];
	prev_work[k] = cur_work[k];
    }
}

void
run_with_interrupts(int tid)
{
    struct timeval now, last;
    int k, done_begin,  do_papi_stop;

    work[tid] = 0;
    count[tid] = 0;

    /* Threshold 0 means run with no interrupts. */
    do_papi_stop = 0;
    if (args.threshold[0] > 0) {
	if (PAPI_overflow(EventSet[tid], args.event[0], args.threshold[0],
			  0, my_handler) != PAPI_OK) {
	    errx(1, "PAPI_overflow failed: %s", args.name[0]);
	}
	if (PAPI_start(EventSet[tid]) != PAPI_OK) {
	    errx(1, "PAPI_start failed");
	}
	do_papi_stop = 1;
    }

    last = time_start;
    done_begin = 0;
    while (state[tid] == RUN) {
	run_flops(1);
	work[tid] += 1;

	/*
	 * Thread zero watches time of day, prints incremental results
	 * and decides when to collect data and when to stop.
	 */
	if (tid == 0) {
	    gettimeofday(&now, NULL);
	    if (time_sub(now, last) >= 1.0) {
		print_stats(now, last);
		last = now;
	    }
	    if (!done_begin && time_sub(now, time_start) >= len_begin) {
		begin_work = 0;
		begin_count = 0;
		for (k = 0; k < args.num_threads; k++) {
		    begin_work += work[k];
		    begin_count += count[k];
		}
		time_begin = now;
		done_begin = 1;
	    }
	    else if (done_begin && time_sub(now, time_begin) >= len_end) {
		end_work = 0;
		end_count = 0;
		for (k = 0; k < args.num_threads; k++) {
		    end_work += work[k];
		    end_count += count[k];
		}
		time_end = now;
		break;
	    }
	}
    }

    if (do_papi_stop) {
	if (PAPI_stop(EventSet[tid], NULL) != PAPI_OK) {
	    warnx("PAPI_stop failed");
	}
    }
}

void
thread_zero(void *data)
{
    long this_work, this_count;
    float evrate, delta_time;
    int k, num;

    if (pthread_setspecific(key, data) != 0) {
	errx(1, "pthread_setspecific failed");
    }
    EventSet[0] = event_set_for_overflow(&args, &my_handler);

    /* Wait for side threads to finish INIT. */
    wait_on_state(INIT);

    max_index = 0;
    for (num = 0; Threshold[num] >= 0; num++) {
	args.threshold[0] = Threshold[num];
	printf("\n%s@%d\n", args.name[0], args.threshold[0]);

	for (k = 0; k < args.num_threads; k++) {
	    work[k] = 0;
	    cur_work[k] = 0;
	    prev_work[k] = 0;
	    count[k] = 0;
	    cur_count[k] = 0;
	    prev_count[k] = 0;
	}
	begin_work = 0;
	begin_count = 0;
	end_work = 0;
	end_count = 0;

	/* launch threads */
	set_state(RUN);
	wait_on_state(RUN);
	gettimeofday(&time_start, NULL);
	run_with_interrupts(0);
	set_state(STOP);
	wait_on_state(STOP);

	this_work = end_work - begin_work;
	this_count = end_count - begin_count;
	evrate = (float) (Threshold[num] * this_count) / (float) this_work;
	delta_time = time_sub(time_end, time_begin);

	base_work = MAX(base_work, this_work);
	base_evrate = MAX(base_evrate, evrate);

	Work[num] = ((float) this_work) / delta_time;
	Intr[num] = ((float) this_count) / delta_time;
	Overhead[num] = 100.0 * (1.0 - ((float) this_work / (float) base_work));

	printf("Average work/sec: %.1f (%.1f), intr/sec: %.1f (%.1f), evrate: %.4e\n"
	       "Overhead: %.1f%%\n",
	       Work[num]/fnum_threads, Work[num],
	       Intr[num]/fnum_threads, Intr[num],
	       evrate, Overhead[num]);

	max_index = num;
	if (Overhead[num] >= MAX_OVER_RATE) {
	    break;
	}
    }

    set_state(EXIT);
}

void *
side_thread(void *data)
{
    int tid = *(int *)data;

    if (pthread_setspecific(key, data) != 0) {
	errx(1, "pthread_setspecific failed");
    }
    EventSet[tid] = event_set_for_overflow(&args, &my_handler);
    ready[tid] = INIT;

    for (;;) {
	if (state[tid] == EXIT) {
	    break;
	}
	else if (state[tid] == RUN) {
	    ready[tid] = RUN;
	    run_with_interrupts(tid);
	    ready[tid] = STOP;
	}
	// usleep(500);
    }

    return NULL;
}

int
main(int argc, char **argv)
{
    pthread_t td[MAX_THREADS];
    int tid[MAX_THREADS];
    int k, opt;

    set_default_args(&args);
    args.prog_time = DEFAULT_TIME;
    opt = parse_args(&args, argc, argv);
    get_papi_events(&args, opt, argc, argv);
    if (args.num_events == 0) {
	TOT_CYC_DEFAULT(args);
    }
    args.prog_time = MAX(args.prog_time, MIN_TIME);
    args.num_events = 1;

    printf("Threads Overhead Test, threads: %d\n", args.num_threads);

    len_begin = 0.25 * (float) args.prog_time;
    len_end = 0.75 * (float) args.prog_time;
    fnum_threads = (float) args.num_threads;

    set_state(INIT);

    for (k = 0; k < MAX_THREADS; k++) {
	tid[k] = k;
    }

    if (PAPI_thread_init(pthread_self) != PAPI_OK) {
        errx(1, "PAPI_thread_init failed");
    }

    if (pthread_key_create(&key, NULL) != 0) {
        errx(1, "pthread key create failed");
    }

    for (k = 1; k < args.num_threads; k++) {
	if (pthread_create(&td[k], NULL, side_thread, &tid[k]) != 0)
	    errx(1, "pthread create failed");
    }
    thread_zero(&td[0]);

    for (k = 1; k < args.num_threads; k++) {
	pthread_join(td[k], NULL);
    }

    printf("\nThreads Overhead Test, threads: %d\n\n", args.num_threads);
    printf("%15s  %10s  %10s  %10s  %12s\n",
	   args.name[0], "Work/sec", "Intr/thr", "Intr/sec", "Overhead %");

    for (k = 0; k <= max_index; k++) {
	printf("%15ld  %10.1f  %10.1f  %10.1f  %10.1f\n",
	       Threshold[k], Work[k], Intr[k]/fnum_threads, Intr[k], Overhead[k]);
    }
    printf("\n");

    return 0;
}
