/*
 *  Functions for churning flops, taking cache misses, etc.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  October 2009
 *
 *  $Id: cycles.c 278 2013-01-03 04:17:10Z krentel $
 */

#include <sys/mman.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include "papi-tests.h"

/*
 *  Floating point add, sub, mult, divide, some branch instructions.
 *
 *  These tests are tuned to take approx 1 sec for num = 1000 on my
 *  main test machine (2.25 GHz Intel Xeon 5520).  YMMV a lot.
 */
#define FLOPS_SCALE  78000
int
run_flops(int num)
{
    double y, z, sum;
    int k, x, num_errs;
    long seed;

    num_errs = 0;
    for (k = 1; k <= num; k++) {
	y = 0.0;
	sum = 0.0;
	seed = 1;
	for (x = 1; x < FLOPS_SCALE; x++) {
	    y += 1.0;
	    z = y * y;
	    seed = random_gen(seed);
	    if (seed % 2 == 0) {
		sum += 4.0/z;
	    } else {
		sum -= 4.0/z;
	    }
	}
	if (sum < 5.10 || sum > 5.40) {
	    warnx("%s: sum is out of range: %g", __func__, sum);
	    num_errs++;
	}
    }

    return (num_errs);
}

/*
 *  MMap() an array of memsize Meg and initialize it.
 *  We maintain the invariant addr[k] = k.
 */
#define MEG  (1024 * 1024)
void
init_memory(struct memory_state *mstate, int memsize)
{
    int k, flags;

    mstate->addr = NULL;
    mstate->bytes = memsize * MEG;
    mstate->size = mstate->bytes / sizeof(mstate->addr[0]);
    mstate->seed = 1;
    if (memsize == 0)
	return;

#ifdef MAP_ANONYMOUS
    flags = MAP_PRIVATE | MAP_ANONYMOUS;
#else
    flags = MAP_PRIVATE | MAP_ANON;
#endif
    mstate->addr = mmap(NULL, mstate->bytes, PROT_READ | PROT_WRITE,
			flags, -1, 0);
    if (mstate->addr == MAP_FAILED) {
	errx(1, "%s: mmap(%d meg) failed", __func__, memsize);
    }
    for (k = 0; k < mstate->size; k++) {
	mstate->addr[k] = k;
    }
}

/*
 *  Walk through array in pseudo-random order and generate cache and
 *  TLB misses.
 */
#define MEM_SCALE  4300
int
run_memory(struct memory_state *mstate, int work)
{
    int r1, r2, r3, r4, w1, w2, w3, w4;
    int k, sum;

    if (mstate->addr == NULL)
	return 1;

    for (k = 1; k <= work * MEM_SCALE; k++)
    {
	mstate->seed = random_gen(mstate->seed);
	r1 = mstate->seed % mstate->size;
	mstate->seed = random_gen(mstate->seed);
	r2 = mstate->seed % mstate->size;
	mstate->seed = random_gen(mstate->seed);
	r3 = mstate->seed % mstate->size;
	mstate->seed = random_gen(mstate->seed);
	r4 = mstate->seed % mstate->size;
	mstate->seed = random_gen(mstate->seed);
	w1 = mstate->seed % mstate->size;
	mstate->seed = random_gen(mstate->seed);
	w2 = mstate->seed % mstate->size;
	mstate->seed = random_gen(mstate->seed);
	w3 = mstate->seed % mstate->size;
	mstate->seed = random_gen(mstate->seed);
	w4 = mstate->seed % mstate->size;

	/* Launch four reads and four writes. */
	sum = mstate->addr[r1]
	    + mstate->addr[r2]
	    + mstate->addr[r3]
	    + mstate->addr[r4];
	mstate->addr[w1] = w1;
	mstate->addr[w2] = w2;
	mstate->addr[w3] = w3;
	mstate->addr[w4] = w4;

	if (sum != r1 + r2 + r3 + r4) {
	    errx(1, "%s: memory corruption near index %d", __func__, r1);
	}
    }

    return 0;
}
