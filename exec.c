/*
 *  Test PAPI across exec().
 *
 *  On some old perfctr systems, if a process execs with an actively
 *  running PAPI_overflow(), then PAPI_library_init() will fail in the
 *  child.  The workaround is to call PAPI_shutdown() before the exec.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  October 2009
 *
 *  $Id: exec.c 278 2013-01-03 04:17:10Z krentel $
 */

#include <sys/time.h>
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <papi.h>
#include "papi-tests.h"

static struct prog_args args;
static int EventSet;

static struct timeval start, last;
static long count = 0;
static long total = 0;

void
my_handler(int EventSet, void *pc, long long ovec, void *context)
{
    count++;
    total++;
}

void
wait_for_time(int len)
{
    struct timeval begin_cycles, now;

    gettimeofday(&begin_cycles, NULL);
    last = begin_cycles;

    count = 0;
    for (;;) {
	run_flops(10);

        gettimeofday(&now, NULL);

        if (now.tv_sec > last.tv_sec) {
	    printf("pid: %d, time: %ld, count = %ld\n",
		   getpid(), now.tv_sec - start.tv_sec, count);
            count = 0;
            last = now;
        }

        if (now.tv_sec >= begin_cycles.tv_sec + len)
            break;
    }
}

void
my_papi_start(void)
{
    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT)
        errx(1, "PAPI_library_init failed");

    EventSet = event_set_for_overflow(&args, &my_handler);

    if (PAPI_start(EventSet) != PAPI_OK)
        errx(1, "PAPI_start failed");
}

/*
 *  For convenience, main() serves as both the parent and the child,
 *  as determined by the command-line arguments.
 *
 *    No args: exec() with active PAPI overflow.
 *
 *    One arg: add PAPI_stop() before exec().
 *
 *    Two args: add PAPI_shutdown().
 *
 *    Three args: be the child and don't exec().
 */
int
main(int argc, char **argv)
{
    int parent = (argc < 4);

    set_default_args(&args);
    TOT_CYC_DEFAULT(args);

    if (argc >= 2 && strcmp(argv[1], "-h") == 0) {
	usage(argv[0]);
	exit(0);
    }
    if (argc == 1)
	printf("Exec test, with active PAPI_overflow across exec\n");
    else if (argc == 2)
	printf("Exec test, with PAPI_stop workaround\n");
    else if (argc == 3)
	printf("Exec test, with PAPI_shutdown workaround\n");
    if (parent)
	print_event_list(&args);

    gettimeofday(&start, NULL);

    /*
     * Have the child wait a short time to see if the parent's PAPI
     * interrupts the child.
     */
    if (! parent) {
	wait_for_time(4);
    }

    printf("---> PAPI start\n");
    my_papi_start();
    wait_for_time(4);

    if (parent) {
	if (argc >= 2) {
	    printf("---> PAPI stop\n");
	    PAPI_stop(EventSet, NULL);
	}
	if (argc >= 3) {
	    printf("---> PAPI shutdown\n");
	    PAPI_shutdown();
	}
	printf("---> exec\n");
	execl(argv[0], argv[0], "x", "x", "x", NULL);
	errx(1, "execl failed");
    }

    /* child process */
    printf("---> PAPI shutdown\n");
    PAPI_stop(EventSet, NULL);
    PAPI_shutdown();

    EXIT_PASS_FAIL(total > 50);
    return (0);
}
