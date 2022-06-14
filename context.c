/*
 *  Verify that PAPI_overflow() provides correct program counter
 *  (sig-context) info.
 *
 *  Put assembler labels between loops and check if the number of
 *  interrupts between two fences is roughly proportional to its
 *  running time.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  October 2009
 *
 *  $Id: context.c 281 2013-07-11 19:40:04Z krentel $
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

#define NUM_RANGES  4

extern void fence0;
extern void fence1;
extern void fence2;
extern void fence3;
extern void fence4;

static struct prog_args args;

static void *fence[NUM_RANGES + 1];
static int count[NUM_RANGES + 1];

static int total = 0;

/*
 *  Count interrupts with PCs between fence0 and fence1 in count[1],
 *  etc, with out of bounds PCs in count[0].
 */
void
my_handler(int EventSet, void *pc, long long ovec, void *context)
{
    int k;

    total++;
    for (k = 0; k <= NUM_RANGES; k++) {
	if (pc < fence[k]) {
	    count[k]++;
	    return;
	}
    }
    count[0]++;
}

void
run_fence(void)
{
    double x, sum;

    asm volatile (".globl  fence0");
    asm volatile ("fence0:");

    sum = 0.0;
    for (x = 1.0; x < 1000000.0; x += 1.0)
	sum += x;
    if (sum < 1e10)
	warnx("sum is out of range: %g", sum);

    asm volatile (".globl  fence1");
    asm volatile ("fence1:");

    sum = 0.0;
    for (x = 1.0; x < 2000000.0; x += 2.0)
	sum += x;
    if (sum < 1e10)
	warnx("sum is out of range: %g", sum);

    asm volatile (".globl  fence2");
    asm volatile ("fence2:");

    sum = 0.0;
    for (x = 1.0; x < 3000000.0; x += 3.0)
	sum += x;
    if (sum < 1e10)
	warnx("sum is out of range: %g", sum);

    asm volatile (".globl  fence3");
    asm volatile ("fence3:");

    sum = 0.0;
    for (x = 1.0; x < 4000000.0; x += 4.0)
	sum += x;
    if (sum < 1e10)
	warnx("sum is out of range: %g", sum);

    asm volatile (".globl  fence4");
    asm volatile ("fence4:");
}

void
run_test(void)
{
    struct timeval start, now;
    int EventSet, k;

    fence[0] = &fence0;
    fence[1] = &fence1;
    fence[2] = &fence2;
    fence[3] = &fence3;
    fence[4] = &fence4;

    for (k = 0; k <= NUM_RANGES; k++)
	count[k] = 0;

    gettimeofday(&start, NULL);

    EventSet = event_set_for_overflow(&args, &my_handler);
    if (PAPI_start(EventSet) != PAPI_OK)
	errx(1, "PAPI_start failed");

    do {
	for (k = 0; k < 10; k++)
	    run_fence();
	gettimeofday(&now, NULL);
    }
    while (time_sub(now, start) < args.prog_time);

    PAPI_stop(EventSet, NULL);
}

int
main(int argc, char **argv)
{
    int k, opt, pass;

    set_default_args(&args);
    args.prog_time = 10;
    opt = parse_args(&args, argc, argv);
    get_papi_events(&args, opt, argc, argv);
    if (args.num_events == 0) {
	TOT_CYC_DEFAULT(args);
    }

    printf("Program Counter Context test, time: %d\n", args.prog_time);
    print_event_list(&args);

    run_test();

    pass = 1;
    for (k = 1; k <= NUM_RANGES; k++) {
	printf("Range %d..%d:  %d\n", k - 1, k, count[k]);
	if (count[k] < 0.80 * (total / NUM_RANGES))
	    pass = 0;
    }
    printf("Out of bounds:  %d\n", count[0]);
    if (total < 50 || count[0] > 0.10 * total) {
	pass = 0;
    }

    EXIT_PASS_FAIL(pass);
}
