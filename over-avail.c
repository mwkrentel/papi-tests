/*
 *  Scan the PAPI presets, try to trigger overflows and summarize in
 *  which events we got interrupts.
 *
 *  Note: 'Failed' only means that this test program failed to trigger
 *  overflows for that event, not necessarily that PAPI_overflow() is
 *  broken for that event.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  October 2009
 *
 *  $Id: over-avail.c 281 2013-07-11 19:40:04Z krentel $
 */

#include <sys/time.h>
#include <sys/types.h>
#include <err.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <papi.h>
#include "papi-tests.h"

#define NEEDED_TO_PASS  50

#define NOT_AVAIL  "Not available"
#define STRANGE    "Strange failure"
#define DERIVED  "Derived"
#define PASSED   "Passed"
#define FAILED   "Failed"

#ifndef PAPI_MAX_PRESET_EVENTS
#define PAPI_MAX_PRESET_EVENTS  250
#endif

struct {
    char *name;
    char *desc;
    char *verdict;
    int  avail;
    int  over;
    int  pass;
} event[PAPI_MAX_PRESET_EVENTS];

static struct prog_args args;
static struct memory_state memstate;

static volatile long count = 0;
static volatile long total = 0;

static int total_events = 0;
static int num_avail = 0;
static int num_overflow = 0;
static int num_passed = 0;

void
my_handler(int EventSet, void *pc, long long ovec, void *context)
{
    count++;
    total++;
}

/*
 * Returns: 1 if the event code is a derived event.
 * The papi_avail(1) utility shows how to do this.
 */
int
is_derived(int ev)
{
  PAPI_event_info_t info;

  if (PAPI_get_event_info(ev, &info) != PAPI_OK
      || info.derived == NULL) {
    return 1;
  }
  if (info.count == 1
      || strlen(info.derived) == 0
      || strcmp(info.derived, "NOT_DERIVED") == 0
      || strcmp(info.derived, "DERIVED_CMPD") == 0) {
    return 0;
  }
  return 1;
}

/*
 * For a given PAPI event, report if it is available, available for
 * overflow, and whether our test programs can trigger overflows.
 */
void
run_test(int ev)
{
    struct timeval start, last, now;
    PAPI_event_info_t info;
    int EventSet;
    char name[500];
    int nev = total_events++;

    memset(&event[nev], 0, sizeof(event[0]));

    PAPI_event_code_to_name(ev, name);
    event[nev].name = strdup(name);
    printf("\n%s\n", name);

    PAPI_get_event_info(ev, &info);
    event[nev].desc = strdup(info.long_descr);

    if (PAPI_query_event(ev) != PAPI_OK) {
	event[nev].verdict = NOT_AVAIL;
	printf("%s\n", NOT_AVAIL);
	return;
    }
    event[nev].avail = 1;
    num_avail++;

    if (is_derived(ev)) {
	event[nev].verdict = DERIVED;
	printf("%s\n", DERIVED);
	return;
    }

    EventSet = PAPI_NULL;
    if (PAPI_create_eventset(&EventSet) != PAPI_OK) {
        errx(1, "PAPI_create_eventset failed");
    }
    if (PAPI_add_event(EventSet, ev) != PAPI_OK) {
	event[nev].verdict = STRANGE;
	printf("%s: PAPI_add_event() failed\n", STRANGE);
	goto cleanup;
    }
    if (PAPI_overflow(EventSet, ev, args.overflow, 0, my_handler) != PAPI_OK) {
	event[nev].verdict = STRANGE;
	printf("%s: PAPI_overflow() failed\n", STRANGE);
	goto cleanup;
    }

    gettimeofday(&start, NULL);
    last = start;

    if (PAPI_start(EventSet) != PAPI_OK) {
	event[nev].verdict = STRANGE;
	printf("%s: PAPI_start() failed\n", STRANGE);
	goto cleanup;
    }
    event[nev].over = 1;
    num_overflow++;

    count = 0;
    total = 0;
    memstate.seed = 1;
    do {
	run_flops(10);
	if (args.memsize > 0) {
	    run_memory(&memstate, 10);
	}

	gettimeofday(&now, NULL);
	if (time_sub(now, last) >= 1.0) {
	    printf("time: %.1f, count: %ld, total: %ld\n",
		   time_sub(now, start), count, total);
	    count = 0;
	    last = now;
	}
	if (time_sub(now, start) >= 2.5 && total >= NEEDED_TO_PASS)
	    break;
    }
    while (time_sub(now, start) <= args.prog_time + 0.5);

    PAPI_stop(EventSet, NULL);

    if (total >= NEEDED_TO_PASS) {
	event[nev].verdict = PASSED;
	event[nev].pass = 1;
	num_passed++;
    } else {
	event[nev].verdict = FAILED;
    }
    printf("%s\n", event[nev].verdict);

cleanup:
    PAPI_cleanup_eventset(EventSet);
    PAPI_destroy_eventset(&EventSet);
}

int
main(int argc, char **argv)
{
    int k, ev, opt, ret;

    set_default_args(&args);
    args.prog_time = 30;
    args.overflow = 100000;
    opt = parse_args(&args, argc, argv);
    get_papi_events(&args, opt, argc, argv);

    printf("Overflow Available test, threshold: %d, time: %d\n",
	   args.overflow, args.prog_time);

    init_memory(&memstate, args.memsize);
    ev = PAPI_PRESET_MASK;
#ifdef PAPI_ENUM_FIRST
    PAPI_enum_event(&ev, PAPI_ENUM_FIRST);
#endif
    do {
	run_test(ev);
	ret = PAPI_enum_event(&ev, PAPI_ENUM_EVENTS);
    }
    while (ret == PAPI_OK);

    printf("\n----------------------------------\n"
	   "Events Not Available for Overflow\n"
	   "----------------------------------\n\n");

    for (k = 0; k < total_events; k++) {
	if (! event[k].avail) {
	    printf("%s \t%s \t%s\n",
		   event[k].name, event[k].verdict, event[k].desc);
	}
    }
    printf("\n");
    for (k = 0; k < total_events; k++) {
	if (event[k].avail && !event[k].over) {
	    printf("%s \t%s \t%s\n",
		   event[k].name, event[k].verdict, event[k].desc);
	}
    }

    printf("\n------------------------------\n"
	   "Events Available for Overflow\n"
	   "------------------------------\n\n"
	   "Note: '%s' only means that this test program failed to trigger\n"
	   "overflows for that event, not necessarily that PAPI_overflow() is\n"
	   "broken for that event.\n\n", FAILED);

    for (k = 0; k < total_events; k++) {
	if (event[k].avail && event[k].over && !event[k].pass) {
	    printf("%s \t%s \t %s\n",
		   event[k].name, event[k].verdict, event[k].desc);
	}
    }
    printf("\n");
    for (k = 0; k < total_events; k++) {
	if (event[k].avail && event[k].pass) {
	    printf("%s \t%s \t %s\n",
		   event[k].name, event[k].verdict, event[k].desc);
	}
    }

    printf("\nTotal PAPI Presets: %d, Available: %d, "
	   "Overflow: %d, Passed: %d\n",
	   total_events, num_avail, num_overflow, num_passed);

    return (0);
}
