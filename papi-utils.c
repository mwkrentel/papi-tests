/*
 *  PAPI-related utility functions.
 *
 *  Copyright (c) 2009-2013, Rice University.
 *  See the file LICENSE for details.
 *
 *  Mark W. Krentel, Rice University
 *  October 2009
 *
 *  $Id: papi-utils.c 281 2013-07-11 19:40:04Z krentel $
 */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <papi.h>
#include "papi-tests.h"

void
get_papi_events(struct prog_args *args, int optind, int argc, char **argv)
{
    char *p, *colon;
    int k, ret, nev;

    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
	errx(1, "PAPI_library_init failed");
    }

    /*
     * Remaining args are EVENT or EVENT:THRESHOLD.
     * We allow : or @ separators.
     */
    nev = 0;
    for (k = optind; k < argc; k++) {
	if (nev >= MAX_EVENTS) {
	    errx(1, "too many events: %d", nev);
	}
	args->name[nev] = strdup(argv[k]);
	args->threshold[nev] = args->overflow;
	colon = NULL;
	for (p = args->name[nev]; *p != 0; p++) {
	    if (*p == ':' || *p == '@')
		colon = p;
	}
	if (colon != NULL) {
	    *colon = 0;
	    ret = sscanf(colon + 1, "%d", &args->threshold[nev]);
	    if (ret < 1 || args->threshold[nev] < 100) {
		errx(1, "invalid argument for threshold: %s", colon + 1);
	    }
	}
	if (PAPI_event_name_to_code(args->name[nev], &args->event[nev])
	    != PAPI_OK) {
	    errx(1, "invalid PAPI event: %s", args->name[nev]);
	}
	nev++;
    }
    args->num_events = nev;
}

void
print_event_list(struct prog_args *args)
{
    int k;

    for (k = 0; k < args->num_events; k++) {
	printf("%s@%d", args->name[k], args->threshold[k]);
	if (k < args->num_events - 1)
	    printf("  ");
    }
    printf("\n");
}

int
event_set_for_overflow(struct prog_args *args, papi_handler_t *handler)
{
    int EventSet, k;
    char name[500];

    EventSet = PAPI_NULL;

    if (PAPI_create_eventset(&EventSet) != PAPI_OK) {
	errx(1, "PAPI_create_eventset failed");
    }

    for (k = 0; k < args->num_events; k++) {
	if (PAPI_add_event(EventSet, args->event[k]) != PAPI_OK) {
	    PAPI_event_code_to_name(args->event[k], name);
	    errx(1, "PAPI_add_event failed: %s", name);
	}
    }

    for (k = 0; k < args->num_events; k++) {
	if (PAPI_overflow(EventSet, args->event[k], args->threshold[k], 0,
			  handler) != PAPI_OK) {
	    PAPI_event_code_to_name(args->event[k], name);
	    errx(1, "PAPI_overflow failed: %s", name);
	}
    }

    return EventSet;
}
