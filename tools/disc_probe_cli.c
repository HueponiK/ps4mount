/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "disc_probe.h"
int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : "/mnt/disc";
    disc_probe_result_t r;
    int rc = disc_probe_scan(root, stdout, &r);
    return rc ? 2 : r.errors ? 1 : r.candidates ? 0 : 3;
}
