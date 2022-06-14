/*
 *  Stress test for profile timers with threads.
 *
 *  Test the behavior of setitimer() and POSIX real-time timers in
 *  threaded programs for distribution of signals and suitability for
 *  profiling.  Itimers are process-wide, so it's essentially
 *  arbitrary which thread receives the signals.  However, on some
 *  systems, the signals are distributed evenly enough among the
 *  threads to be useful for profiling.  POSIX timers with the
 *  SIGEV_THREAD_ID notify method are thread-specific, but that's a
 *  Linux extension.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  October 2012
 *
 *  $Id: itimer.c 281 2013-07-11 19:40:04Z krentel $
 */

#include <sys/types.h>
#include <sys/time.h>
#include <err.h>
#include <error.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#if defined(RTIMER) || defined(CTIMER)
#include <sys/syscall.h>
#endif

#include "papi-tests.h"

#ifdef ITIMER
#define NAME  "Itimer"
#define PROF_SIGNAL  SIGPROF
#define ITIMER_TYPE  ITIMER_PROF
#else
#ifdef RTIMER
#define NAME  "Real-Time"
#define CLOCK_TYPE  CLOCK_REALTIME
#endif
#ifdef CTIMER
#define NAME  "CPU-Time"
#define CLOCK_TYPE  CLOCK_THREAD_CPUTIME_ID
#endif
#define PROF_SIGNAL   (SIGRTMIN + 4)
#define NOTIFY_METHOD  SIGEV_THREAD_ID
#endif

static struct prog_args args;
static pthread_key_t key;

static struct min_max_report rep[MAX_THREADS];

static volatile long count[MAX_THREADS];
static volatile int done = 0;

#if defined(RTIMER) || defined(CTIMER)
static timer_t timerid[MAX_THREADS];
static struct sigevent sev[MAX_THREADS];
#endif

static struct itimerval itval_start;
static struct itimerval itval_stop;

static struct itimerspec itspec_start;
static struct itimerspec itspec_stop;

int
start_timer(int tid)
{
#ifdef ITIMER
    return setitimer(ITIMER_TYPE, &itval_start, NULL);
#else
    return timer_settime(timerid[tid], 0, &itspec_start, NULL);
#endif
}

int
stop_timer(int tid)
{
#ifdef ITIMER
    return setitimer(ITIMER_TYPE, &itval_stop, NULL);
#else
    return timer_settime(timerid[tid], 0, &itspec_stop, NULL);
#endif
}

void
my_handler(int sig, siginfo_t *info, void *context)
{
    int tid = *(int *)pthread_getspecific(key);

    if (tid < 0 || tid >= args.num_threads) {
	warnx("thread id from getspecific out of bounds: %d", tid);
	return;
    }
    count[tid]++;

    if (args.manual_restart) {
	if (start_timer(tid) != 0) {
            err(1, "timer restart failed in handler");
	}
    }
}

/*
 *  Sleep for msec milliseconds.  Need to restart usleep() for the
 *  case that it's interrupted by the profiling signal.
 *
 *  This is useful for comparing realtime and cputime.
 */
void
run_sleep(int msec)
{
    struct timeval start, now;

    gettimeofday(&start, NULL);
    for (;;) {
	usleep(500);
	gettimeofday(&now, NULL);
	if (1000000 * (now.tv_sec - start.tv_sec)
	    + (now.tv_usec - start.tv_usec) >= 1000 * msec) {
	    break;
	}
    }
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
	if (start_timer(tid) != 0) {
	    err(1, "timer start failed");
	}
	my_start = 1;
    }

    num_errs = 0;
    do {
	if (! my_start) {
	    gettimeofday(&now, NULL);
	    if (time_sub(now, start) >= args.stagger_delay * tid) {
		printf("===> starting timer in thread %d\n", tid);
		if (start_timer(tid) != 0) {
		    err(1, "timer start failed");
		}
		my_start = 1;
	    }
	}

	count[tid] = 0;
	work = 0;
	do {
	    num_errs += run_flops(10);
	    work += 10;
	    if (args.sleep) {
		run_sleep(20);
		work += 20;
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

    if (stop_timer(tid) != 0) {
	warnx("timer stop failed");
    }

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

    if (pthread_setspecific(key, data) != 0) {
	errx(1, "pthread_setspecific failed");
    }

#if defined(RTIMER) || defined(CTIMER)
    memset(&sev[tid], 0, sizeof(struct sigevent));
    sev[tid].sigev_notify = NOTIFY_METHOD;
    sev[tid].sigev_signo = PROF_SIGNAL;
    sev[tid].sigev_value.sival_ptr = &timerid[tid];
    sev[tid]._sigev_un._tid = syscall(SYS_gettid);

    if (timer_create(CLOCK_TYPE, &sev[tid], &timerid[tid]) != 0) {
        err(1, "timer_create failed");
    }
#endif

    run_test(tid);
    done = 1;

    return NULL;
}

int
main(int argc, char **argv)
{
    long first_sec, first_usec, repeat_sec, repeat_usec;
    struct sigaction act;
    pthread_t td[MAX_THREADS];
    int tid[MAX_THREADS];
    int k, pass;

    set_default_args(&args);
    args.num_threads = DEFAULT_NUM_THREADS;
    k = parse_args(&args, argc, argv);
    args.prog_time = MAX(args.prog_time, 15);

    if (k >= argc || sscanf(argv[k], "%ld", &first_sec) < 1) {
	usage(argv[0]);
	exit(1);
    }
    k++;
    if (k >= argc || sscanf(argv[k], "%ld", &first_usec) < 1) {
	usage(argv[0]);
	exit(1);
    }
    k++;
    if (k + 1 >= argc || sscanf(argv[k], "%ld", &repeat_sec) < 1
	|| sscanf(argv[k+1], "%ld", &repeat_usec) < 1) {
	repeat_sec = first_sec;
	repeat_usec = first_usec;
    }

    printf("%s Stress test, time: %d, threads: %d\n",
	   NAME, args.prog_time, args.num_threads);
    printf("mode: %s, value: %ld.%ld, repeat: %ld.%ld\n",
	   (args.manual_restart ? "manual-restart" : "auto-repeat"),
	   first_sec, first_usec, repeat_sec, repeat_usec);

    itval_start.it_value.tv_sec = first_sec;
    itval_start.it_value.tv_usec = first_usec;
    itval_start.it_interval.tv_sec = repeat_sec;
    itval_start.it_interval.tv_usec = repeat_usec;

    itspec_start.it_value.tv_sec = first_sec;
    itspec_start.it_value.tv_nsec = 1000 * first_usec;
    itspec_start.it_interval.tv_sec = repeat_sec;
    itspec_start.it_interval.tv_nsec = 1000 * repeat_usec;

    memset(&itval_stop, 0, sizeof(itval_stop));
    memset(&itspec_stop, 0, sizeof(itspec_stop));

    for (k = 0; k < MAX_THREADS; k++) {
	tid[k] = k;
    }

    if (pthread_key_create(&key, NULL) != 0) {
        errx(1, "pthread key create failed");
    }

    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = my_handler;
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(PROF_SIGNAL, &act, NULL) != 0) {
	err(1, "sigaction failed");
    }

    for (k = 1; k < args.num_threads; k++) {
	if (pthread_create(&td[k], NULL, my_thread, &tid[k]) != 0)
	    errx(1, "pthread create failed");
    }
    my_thread(&tid[0]);

    for (k = 1; k < args.num_threads; k++) {
	pthread_join(td[k], NULL);
    }

    printf("%s Stress test, time: %d, threads: %d\n",
	   NAME, args.prog_time, args.num_threads);
    printf("mode: %s, value: %ld.%ld, repeat: %ld.%ld\n",
	   (args.manual_restart ? "manual-restart" : "auto-repeat"),
	   first_sec, first_usec, repeat_sec, repeat_usec);

    pass = 1;
    for (k = 0; k < args.num_threads; k++) {
	printf("tid: %d, min: %ld, avg: %.1f, max: %ld\n",
	       k, rep[k].min, rep[k].avg, rep[k].max);
	pass = pass && rep[k].pass;
    }

    EXIT_PASS_FAIL(pass);
}
