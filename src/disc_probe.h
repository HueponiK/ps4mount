/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PS4MOUNT_DISC_PROBE_H
#define PS4MOUNT_DISC_PROBE_H

#include <stdio.h>

typedef struct {
    unsigned candidates;    /* CUSA directories with an app.pkg */
    unsigned valid_headers; /* readable and structurally plausible PS4 PKG header */
    unsigned pfs_reads;     /* successful direct read probes of the PFS area */
    unsigned errors;        /* invalid packages or unsuccessful reads */
} disc_probe_result_t;

/* Read-only optical package inspection. No installs, mounts, registration or licenses.
 * root is the disc filesystem root, expected to contain app/CUSAxxxxx/app.pkg.
 * Returns -1 if app/ cannot be opened; 0 otherwise. */
int disc_probe_scan(const char *root, FILE *log, disc_probe_result_t *out);

#endif
