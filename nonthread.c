/*
 *  Stress test for non-threaded PAPI interrupts.
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
 *  $Id: nonthread.c 281 2013-07-11 19:40:04Z krentel $
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

static struct prog_args args;
static struct memory_state memstate;
static int EventSet;

static struct min_max_report rep;
static volatile long count = 0;

void
my_handler(int EventSet, void *pc, long long ovec, void *context)
{
    count++;
}

/*
 *  Compute min, max, avg number of interrupts per segment of work
 *  (roughly 1-2 sec).  Declare SUCCESS if min and max are within 25%
 *  of average, and interrupts don't just up and disappear.
 */
void
run_test(void)
{
    struct timeval start, nonzero, now, last;
    char *eol = (args.verbose) ? ", " : "\n";
    int work, num_errs;

    INIT_REPORT(rep);

    gettimeofday(&start, NULL);
    nonzero = start;
    last = start;

    if (PAPI_start(EventSet) != PAPI_OK)
	errx(1, "PAPI_start failed");

    num_errs = 0;
    memstate.seed = 1;
    do {
	count = 0;
	work = 0;
	do {
	    num_errs += run_flops(5);
	    work += 5;
	    if (args.memsize > 0) {
		num_errs += run_memory(&memstate, 5);
		work += 5;
	    }
	}
	while (work < args.work);

	gettimeofday(&now, NULL);
	printf("time: %.1f, work: %d, count: %ld%s",
	       time_sub(now, start), work, count, eol);
	if (args.verbose) {
	    float fcount = (float) count;
	    float fwork = (float) work;
	    float delta_t = time_sub(now, last);
	    printf("intr/sec: %.2f, intr/Kwork: %.2f, work/sec: %.2f\n",
		   fcount/delta_t, 1000.0*fcount/fwork, fwork/delta_t);
	}
	last = now;

	if (time_sub(now, start) > 5.0) {
	    ADD_TO_REPORT(rep, count);
	}
	if (count > 0) {
	    nonzero = now;
	}
	if (time_sub(now, nonzero) > 20.0) {
	    warnx("interrupts have died");
	    num_errs++;
	    break;
	}
    }
    while (time_sub(now, start) <= args.prog_time);

    PAPI_stop(EventSet, NULL);

    rep.avg = rep.total / (float)rep.num;
    rep.pass = (num_errs == 0) && (rep.min > 0.75 * rep.avg)
	&& (rep.max < 1.25 * rep.avg);
}

int
main(int argc, char **argv)
{
    int opt;

    set_default_args(&args);
    opt = parse_args(&args, argc, argv);
    get_papi_events(&args, opt, argc, argv);
    if (args.num_events == 0) {
	TOT_CYC_DEFAULT(args);
    }
    args.prog_time = MAX(args.prog_time, 15);

    printf("Nonthread Stress test, time: %d\n", args.prog_time);
    print_event_list(&args);

    EventSet = event_set_for_overflow(&args, &my_handler);
    init_memory(&memstate, args.memsize);

    run_test();

    if (args.verbose) {
	printf("\nNonthread Stress test, time: %d\n", args.prog_time);
	print_event_list(&args);
    }
    printf("min: %ld, avg: %.1f, max: %ld\n",
	   rep.min, rep.avg, rep.max);

    EXIT_PASS_FAIL(rep.pass);
}
