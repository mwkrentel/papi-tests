/*
 *  Stress test for doing work inside an overflow handler.
 *
 *  This program checks that work done in the handler is not counted
 *  towards the next overflow.  If the counters are not reset in the
 *  handler, and if the amount of work in the handler is too long
 *  relative to the overflow threshold, then interrupts stack up and
 *  the main program makes no progress.  If interrupts stack up too
 *  much, then the process dies on SIGIO with the message "I/O
 *  possible."
 *
 *  For example, './handler -x 50 PAPI_TOT_CYC:50000' is usually
 *  sufficient to trigger the "I/O possible" bug within seconds.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  November 2010
 *
 *  $Id: handler.c 278 2013-01-03 04:17:10Z krentel $
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
static int EventSet;

static struct timeval start;
static struct timeval last;

static volatile long count = 0;
static volatile long iter = 1;
static volatile long last_iter = 0;
static int num_mesg = 0;
static int num_errors = 0;

/*
 * Print messages from inside the handler.  The test passes if we
 * never get any 'no progress' interrupts.
 */
void
my_handler(int EventSet, void *pc, long long ovec, void *context)
{
    struct timeval now;
    double x, sum;
    int k;

    count++;

    /*
     * Warn if we get multiple interrupts but no progress in the main
     * loop, and rate limit the messages.
     */
    now.tv_sec = 0;
    if (iter == last_iter
	&& (num_mesg < 5
	    || (num_mesg < 10 && count % 20 == 0)
	    || (num_mesg < 15 && count % 100 == 0)
	    || (num_mesg < 20 && count % 1000 == 0))) {
	gettimeofday(&now, NULL);
	printf("time: %ld, main loop: %ld, count: %ld -- no progress\n",
	       now.tv_sec - start.tv_sec, iter, count);
	num_mesg++;
	num_errors++;
    }
    last_iter = iter;

    /*
     * Display tick message one per second and reset the message
     * count.
     */
    if (now.tv_sec == 0 && count % 20 == 0) {
	gettimeofday(&now, NULL);
    }
    if (now.tv_sec > last.tv_sec) {
	printf("time: %ld, main loop: %ld, count: %ld -- tick\n",
	       now.tv_sec - start.tv_sec, iter, count);
	last = now;
	num_mesg = 0;
    }
    if (now.tv_sec > start.tv_sec + args.prog_time) {
	printf("done, num errors: %d\n", num_errors);
	EXIT_PASS_FAIL(num_errors == 0);
    }

    /*
     * Churn cycles before returning.
     */
    for (k = 1; k <= args.handler_iter; k++) {
	sum = (double) k;
	for (x = 1.0; x < 1000.0; x += 1.0) {
	    sum += x;
	}
	if (sum < 25000.0) {
	    warnx("sum is out of range: %g", sum);
	}
    }
}

/*
 * Churn cycles forever in the main program and count loop iterations
 * to see if it makes progress.
 */
void
run_test(void)
{
    double x, sum;
    long k;

    gettimeofday(&start, NULL);
    last = start;

    if (PAPI_start(EventSet) != PAPI_OK)
        errx(1, "PAPI_start failed");

    for (k = 1;; k++) {
        sum = (double) k;
        for (x = 1.0; x < 500000.0; x += 1.0) {
	    sum += x;
            iter++;
	}
        if (sum < 25000000.0) {
            warnx("sum is out of range: %g", sum);
        }
    }
}

int
main(int argc, char **argv)
{
    int opt;

    set_default_args(&args);
    args.overflow = 50000;
    args.handler_iter = DEFAULT_HANDLER_ITER;
    opt = parse_args(&args, argc, argv);
    get_papi_events(&args, opt, argc, argv);
    if (args.num_events == 0) {
	TOT_CYC_DEFAULT(args);
    }
    args.prog_time = MAX(args.prog_time, 10);

    printf("Signal Handler Work test, time: %d, work in handler: %d\n",
	   args.prog_time, args.handler_iter);
    print_event_list(&args);

    EventSet = event_set_for_overflow(&args, &my_handler);

    run_test();
    printf("never get here\n");

    return 0;
}
