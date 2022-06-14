/*
 *  PAPI overhead and throttle test.
 *
 *  This program measures the amount of overhead from PAPI at higher
 *  and higher interrupt rates (upwards of 100,000/sec).  It also
 *  tests if the kernel throttles the interrupt rate and by how much.
 *
 *  Overflow is the percentage loss in the amount of work per second,
 *  compared to the rate with no interrupts.  Throttling is the
 *  percentage loss in the event rate (interrupts times threshold
 *  divided by work), compared to a moderate rate of interrupts
 *  (50-100/sec).  See the README file for more explanation.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  September 2010
 *
 *  $Id: throttle.c 281 2013-07-11 19:40:04Z krentel $
 */

#include <sys/time.h>
#include <sys/types.h>
#include <err.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <papi.h>
#include "papi-tests.h"

#define DEFAULT_TIME  20
#define MIN_TIME      10
#define SIZE  25

static long Threshold[SIZE] = {
           0,  200000000,  100000000,
    50000000,   20000000,   10000000,
     5000000,    2000000,    1000000,
      500000,     200000,     100000,
       50000,      20000,      10000,
	5000,         -1
};

static float Work[SIZE];
static float Intr[SIZE];
static float Overhead[SIZE];
static float Throttle[SIZE];
static int   Ok[SIZE];

static struct prog_args args;
static int EventSet;

static long base_work = -1;
static float base_evrate = -1.0;

static volatile long count = 0;

void
my_handler(int EventSet, void *pc, long long ovec, void *context)
{
    count++;
}

void
run_test(void)
{
    struct timeval start, now;
    long work, min_work, max_work, total_work;
    long min_intr, max_intr, total_intr;
    int k, ok, tick, warmup;
    float evrate;

    warmup = MAX(args.prog_time/5, 5);

    for (k = 0; Threshold[k] >= 0; k++) {
	args.threshold[0] = Threshold[k];
	printf("\n%s@%d\n", args.name[0], args.threshold[0]);

	count = 0;
	work = 0;
	min_work = 500000000;
	max_work = 0;
	min_intr = 500000000;
	max_intr = 0;
	total_work = 0;
	total_intr = 0;
	tick = 0;

	gettimeofday(&start, NULL);

	/* Threshold 0 means run with no interrupts. */
	if (Threshold[k] > 0) {
	    EventSet = event_set_for_overflow(&args, &my_handler);
	    if (PAPI_start(EventSet) != PAPI_OK)
		errx(1, "PAPI_start failed");
	}

	while (tick < warmup + args.prog_time) {
	    run_flops(10);
	    work += 10;
	    gettimeofday(&now, NULL);
	    if (time_sub(now, start) >= 1.0 + (float)tick) {
		tick++;
		evrate = (float)Threshold[k] * count / (float)work;
		printf("time: %.1f, work: %ld, intr: %ld, evrate: %.4e\n",
		       time_sub(now, start), work, count, evrate);
		if (tick > warmup) {
		    min_work = MIN(min_work, work);
		    max_work = MAX(max_work, work);
		    min_intr = MIN(min_intr, count);
		    max_intr = MAX(max_intr, count);
		    total_work += work;
		    total_intr += count;
		}
		count = 0;
		work = 0;
	    }
	}

	PAPI_stop(EventSet, NULL);

	/* Base work rate is computed at 0 interrupts (first run).
	 * Base event rate is computed at about 50-100 interrupts per
	 * second.  Both are increased if we get a higher value later.
	 */
	base_work = MAX(base_work, total_work);
	evrate = (float)Threshold[k] * total_intr / (float)total_work;
	if (total_intr > 50 * args.prog_time) {
	    base_evrate = MAX(base_evrate, evrate);
	}

	/* Values per second for summary. */
	Work[k] = total_work / (float)args.prog_time;
	Intr[k] = total_intr / (float)args.prog_time;
	Overhead[k] = 100.0 * (1.0 - total_work / (float)base_work);
	Throttle[k] = (base_evrate < 0.0) ? 0.0
	     : 100.0 * (1.0 - evrate / (float)base_evrate);
	Ok[k] = (max_work <= min_work + 35 || (float)max_work <= 1.1 * min_work)
	     && (max_intr <= min_intr + 20 || (float)max_intr <= 1.1 * min_intr);

	printf("Avgerage work: %.1f, intr: %.1f, evrate: %.4e\n",
	       Work[k], Intr[k], evrate);
	printf("Overhead: %.1f%%, Throttle: %.1f%%%s\n",
	       Overhead[k], Throttle[k],
	       Ok[k] ? "" : "  (may be inaccurate)");
    }

    printf("\nOverhead and Throttle test\n");
    printf("\n%15s  %10s  %11s  %12s  %12s\n",
	   args.name[0], "Work/sec", "Intr/sec", "Overhead %", "Throttle %");

    ok = 1;
    for (k = 0; Threshold[k] >= 0; k++) {
	printf("%15ld  %10.1f  %11.1f  %10.1f  %12.1f%s\n",
	       Threshold[k], Work[k], Intr[k], Overhead[k], Throttle[k],
	       Ok[k] ? "" : "  *");
	ok = ok && Ok[k];
    }
    if (! ok) {
	printf("\n* = the process did not get a steady rate of interrupts and the\n"
	       "    results may be inaccurate, probably due to system load.\n");
    }
    printf("\n");
}

int
main(int argc, char **argv)
{
    int opt;

    set_default_args(&args);
    args.prog_time = DEFAULT_TIME;
    opt = parse_args(&args, argc, argv);
    get_papi_events(&args, opt, argc, argv);
    if (args.num_events == 0) {
	TOT_CYC_DEFAULT(args);
    }
    args.prog_time = MAX(args.prog_time, MIN_TIME);
    args.num_events = 1;
    args.threshold[0] = 0;

    printf("Overhead and Throttle test, time: %d\n", args.prog_time);

    run_test();

    return 0;
}
