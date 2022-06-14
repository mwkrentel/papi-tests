/*
 *  Test PAPI across fork().
 *
 *  On some old perfmon systems, if a process forks with an actively
 *  running PAPI_overflow(), then PAPI in the child can interfere with
 *  PAPI in the parent.  When this happens, PAPI interrupts die in the
 *  parent when the child exits.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  October 2009
 *
 *  $Id: fork.c 278 2013-01-03 04:17:10Z krentel $
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

static struct timeval start;
static int parent = 1;
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
    struct timeval begin_cycles, now, last;

    gettimeofday(&begin_cycles, NULL);
    last = begin_cycles;

    count = 0;
    for (;;) {
	run_flops(10);

        gettimeofday(&now, NULL);

        if (now.tv_sec > last.tv_sec) {
	    printf("pid: %d, time: %ld, %s = %ld\n",
		   getpid(), now.tv_sec - start.tv_sec,
		   (parent ? "parent" : "child"), count);
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
 *  Usage: ./fork [exit|exec] [x]
 *
 *  If the first argument is 'exit' or 'exec', then the child
 *  terminates via exit() or exec().
 *
 *  With an extra argument, the parent performs the workaround of
 *  calling PAPI_shutdown() before fork() and resumes PAPI after fork.
 */
int
main(int argc, char **argv)
{
    int do_exec, do_shutdown;
    int n, ret;

    set_default_args(&args);
    TOT_CYC_DEFAULT(args);

    if (argc >= 2 && strcmp(argv[1], "-h") == 0) {
	usage(argv[0]);
	exit(0);
    }
    do_exec = 0;
    do_shutdown = 0;
    n = 1;
    if (argc > 1 && strcasecmp(argv[1], "exit") == 0) {
	do_exec = 0;
	n++;
    } else if (argc > 1 && strcasecmp(argv[1], "exec") == 0) {
	do_exec = 1;
	n++;
    }
    if (argc > n) {
	do_shutdown = 1;
    }

    printf("Fork test: child %s, %s\n",
	   do_exec ? "exec" : "exit",
	   do_shutdown ? "PAPI_shutdown before fork"
		       : "active PAPI_overflow across fork");

    print_event_list(&args);

    gettimeofday(&start, NULL);

    printf("---> parent\n");
    my_papi_start();
    wait_for_time(4);

    if (do_shutdown) {
	printf("---> parent PAPI stop and shutdown\n");
	PAPI_stop(EventSet, NULL);
	PAPI_shutdown();
    }

    ret = fork();
    if (ret < 0)
	errx(1, "fork failed");

    if (ret > 0) {
	/*
	 * Parent process.
	 */
	if (do_shutdown) {
	    printf("---> parent PAPI restart\n");
	    my_papi_start();
	}
	wait_for_time(14 + (do_exec ? 4 : 0));
	total = 0;
	wait_for_time(4);
	printf("---> parent exit\n");
	EXIT_PASS_FAIL(total > 50);
    }

    /*
     * Child process.
     */
    parent = 0;
    printf("---> child\n");
    wait_for_time(4);

    printf("---> child PAPI start\n");
    my_papi_start();
    wait_for_time(4);

    printf("---> child PAPI stop and shutdown\n");
    PAPI_stop(EventSet, NULL);
    PAPI_shutdown();
    wait_for_time(4);

    if (do_exec) {
	printf("---> child exec (/bin/sh -c 'sleep ; echo')\n");
	execl("/bin/sh", "/bin/sh", "-c", "sleep 4 ; echo '---> child exit'", NULL);
	errx(1, "execl failed");
    }

    printf("---> child exit\n");
    return (0);
}
