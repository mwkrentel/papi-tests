/*
 *  Stress test for PAPI interrupts from multiple events.
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
 *  $Id: mult-events.c 281 2013-07-11 19:40:04Z krentel $
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

static struct min_max_report rep[MAX_EVENTS];
static volatile long count[MAX_EVENTS];
static volatile long total;

void
my_handler(int EventSet, void *pc, long long ovec, void *context)
{
    int array[MAX_EVENTS];
    int k, size = MAX_EVENTS;

    if (PAPI_get_overflow_event_index(EventSet, ovec, array, &size)
	!= PAPI_OK) {
	errx(1, "PAPI_get_overflow_event_index failed");
    }
    if (size < 1 || size > args.num_events)
	errx(1, "handler: size out of range: %d", size);

    for (k = 0; k < size; k++) {
	if (array[k] < 0 || array[k] >= args.num_events)
	    errx(1, "handler: array[%d] out of range: %d", k, array[k]);
	count[array[k]]++;
    }
    total++;
}

/*
 *  Compute min, max, avg number of interrupts per segment of work
 *  (roughly 1-2 sec).  Declare SUCCESS if min and max are within 25%
 *  of average, and interrupts don't just up and disappear.
 */
void
run_test(void)
{
    struct timeval start, now;
    struct timeval nonzero[MAX_EVENTS];
    int k, work, num_errs;

    for (k = 0; k < args.num_events; k++) {
	INIT_REPORT(rep[k]);
    }

    gettimeofday(&start, NULL);
    for (k = 0; k < args.num_events; k++) {
	nonzero[k] = start;
    }
    if (PAPI_start(EventSet) != PAPI_OK)
	errx(1, "PAPI_start failed");

    num_errs = 0;
    memstate.seed = 1;
    do {
	for (k = 0; k < args.num_events; k++) {
	    count[k] = 0;
	}
	total = 0;
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
	printf("time: %.1f, work: %d, counts: %ld",
	       time_sub(now, start), args.work, count[0]);
	for (k = 1; k < args.num_events; k++) {
	    printf(", %ld", count[k]);
	}
	printf("  (total %ld)\n", total);

	if (time_sub(now, start) > 5.0) {
	    for (k = 0; k < args.num_events; k++) {
		ADD_TO_REPORT(rep[k], count[k]);
	    }
	}
	for (k = 0; k < args.num_events; k++) {
	    if (count[k] > 0) {
		nonzero[k] = now;
	    }
	    if (time_sub(now, nonzero[k]) > 20.0) {
		warnx("interrupts have died for %s", args.name[k]);
		num_errs++;
		break;
	    }
	}
    }
    while (time_sub(now, start) <= args.prog_time);

    PAPI_stop(EventSet, NULL);

    for (k = 0; k < args.num_events; k++) {
	rep[k].avg = rep[k].total / (float)rep[k].num;
	rep[k].pass = (num_errs == 0) && (rep[k].min > 0.75 * rep[k].avg)
	    && (rep[k].max < 1.25 * rep[k].avg);
    }
}

int
main(int argc, char **argv)
{
    int k, opt, pass;

    set_default_args(&args);
    opt = parse_args(&args, argc, argv);
    get_papi_events(&args, opt, argc, argv);
    if (args.num_events == 0) {
	TOT_CYC_DEFAULT(args);
	args.name[1] = "PAPI_L2_TCM";
	args.event[1] = PAPI_L2_TCM;
	args.threshold[1] = 100000;
	args.name[2] = "PAPI_FP_INS";
	args.event[2] = PAPI_FP_INS;
	args.threshold[2] = 200000;
	args.num_events = 3;
    }
    args.prog_time = MAX(args.prog_time, 15);

    printf("Multiple Events Stress test, time: %d\n", args.prog_time);
    print_event_list(&args);

    init_memory(&memstate, args.memsize);
    EventSet = event_set_for_overflow(&args, &my_handler);

    run_test();

    printf("\nMultiple Events Stress test, time: %d\n", args.prog_time);
    print_event_list(&args);

    pass = 1;
    for (k = 0; k < args.num_events; k++) {
	printf("%s: min: %ld, avg: %.1f, max: %ld\n",
	       args.name[k], rep[k].min, rep[k].avg, rep[k].max);
	pass = pass && rep[k].pass;
    }

    EXIT_PASS_FAIL(pass);
}
