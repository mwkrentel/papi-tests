/*
 *  Non-PAPI utility functions.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  October 2009
 *
 *  $Id: utils.c 281 2013-07-11 19:40:04Z krentel $
 */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "papi-tests.h"

#define OPT_ARG_STR  "1hm:o:p:rs:t:vw:x:z"

void
usage(char *name)
{
    printf("usage: %s [-%s] [EVENT | EVENT@PERIOD] ...\n"
	   "       %s [-%s] sec usec [sec usec]\n\n"
	   "    -1\n"
	   "\tPrint output from one thread only.\n\n"
	   "    -h\n"
	   "\tPrint this usage message.\n\n"
	   "    -m <num>\n"
	   "\tSize of array (per thread) in Megabytes for the memory cache\n"
	   "\ttests.  Must be between 1 and 2000, or else 0 to disable the\n"
	   "\tmemory tests (default %d).\n\n"
	   "    -o <num>\n"
	   "\tThe default overflow threshold (default %d).\n\n"
	   "    -p <num>\n"
	   "\tThe number of pthreads for the threads test (default %d).\n\n"
	   "    -r\n"
	   "\tUse manual restart mode for itimer and rtimer tests.\n\n"
	   "    -s <num>\n"
	   "\tTime in seconds to stagger starting side threads (default %d).\n\n"
	   "    -t <num>\n"
	   "\tTime to run the tests in seconds (default %d).\n\n"
	   "    -v\n"
	   "\tMore verbose output per time step.\n\n"
	   "    -w <num>\n"
	   "\tAmount of work per iteration (default %d).  The unit of work\n"
	   "\tis arbitrary, but 1000 units takes roughly a few seconds.\n\n"
	   "    -x <num>\n"
	   "\tNumber of loop iterations in the overflow handler (default %d).\n"
	   "\tOnly applies to the handler test.\n\n"
	   "    -z\n"
	   "\tAdd sleep (zzz) to the timer tests.\n\n",
	   name, OPT_ARG_STR,
	   name, OPT_ARG_STR,
	   DEFAULT_MEMSIZE,
	   DEFAULT_THRESHOLD,
	   DEFAULT_NUM_THREADS,
	   DEFAULT_STAGGER_DELAY,
	   DEFAULT_PROG_TIME,
	   DEFAULT_WORK,
	   DEFAULT_HANDLER_ITER);

    printf("EVENT can be a PAPI preset event (eg, PAPI_TOT_CYC) or a native event\n"
	   "(eg, UNHALTED_CORE_CYCLES).  PERIOD is the overflow threshold.  The\n"
	   "delimiter between EVENT and PERIOD may be colon (:) or at-sign (@).\n"
	   "\nNot all options apply to every program.\n");
}

void
set_default_args(struct prog_args *args)
{
    memset(args, 0, sizeof(struct prog_args));
    args->prog_time = DEFAULT_PROG_TIME;
    args->num_threads = DEFAULT_NUM_THREADS;
    args->overflow = DEFAULT_THRESHOLD;
    args->work = DEFAULT_WORK;
    args->memsize = DEFAULT_MEMSIZE;
    args->handler_iter = DEFAULT_HANDLER_ITER;
    args->single = 0;
    args->stagger_delay = DEFAULT_STAGGER_DELAY;
    args->verbose = 0;
    args->num_events = 0;
    args->sleep = 0;
}

int
parse_args(struct prog_args *args, int argc, char **argv)
{
    int c, ret;

    optind = 1;
    while ((c = getopt(argc, argv, OPT_ARG_STR)) != -1) {
	switch (c) {

	/* output from single thread */
	case '1':
	    args->single = 1;
	    break;

	/* display help */
	case 'h':
	    usage(argv[0]);
	    exit(0);
	    break;

	/* size of memory array in megs */
	case 'm':
	    ret = sscanf(optarg, "%d", &args->memsize);
	    if (ret < 1 || args->memsize < 0 || args->memsize > 2000) {
		errx(1, "invalid argument for memsize: %s", optarg);
	    }
	    break;

	/* overflow threshold */
	case 'o':
	    ret = sscanf(optarg, "%d", &args->overflow);
	    if (ret < 1 || args->overflow < 100) {
		errx(1, "invalid argument for overflow: %s",
		     optarg);
	    }
	    break;

	/* number of pthreads */
	case 'p':
	    ret = sscanf(optarg, "%d", &args->num_threads);
	    if (ret < 1 || args->num_threads < 1) {
		errx(1, "invalid argument for number of threads: %s",
		     optarg);
	    }
	    break;

	/* manual restart mode */
	case 'r':
	    args->manual_restart = 1;
	    break;

	/* stagger delay in seconds */
	case 's':
	    ret = sscanf(optarg, "%d", &args->stagger_delay);
	    if (ret < 1 || args->stagger_delay < 0) {
		errx(1, "invalid argument for stagger delay: %s", optarg);
	    }
	    break;

	/* program time in seconds */
	case 't':
	    ret = sscanf(optarg, "%d", &args->prog_time);
	    if (ret < 1 || args->prog_time < 0) {
		errx(1, "invalid argument for program time: %s", optarg);
	    }
	    break;

	/* verbose mode per time step */
	case 'v':
	    args->verbose = 1;
	    break;

	/* amount of work per iteration */
	case 'w':
	    ret = sscanf(optarg, "%d", &args->work);
	    if (ret < 1 || args->work < 1) {
		errx(1, "invalid argument for work per iteration: %s", optarg);
	    }
	    break;

	/* number of loop iterations in sig handler */
	case 'x':
	    ret = sscanf(optarg, "%d", &args->handler_iter);
	    if (ret < 1 || args->handler_iter < 0) {
		errx(1, "invalid argument for iterations in sig handler: %s", optarg);
	    }
	    break;

	/* sleep (zzz) */
	case 'z':
	    args->sleep = 1;
	    break;

	default:
	    usage(argv[0]);
	    exit(1);
	}
    }

    return optind;
}
