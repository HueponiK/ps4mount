/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "disc_probe.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void put_be32(unsigned char *p, uint32_t n) {
    p[0] = (unsigned char)(n >> 24); p[1] = (unsigned char)(n >> 16);
    p[2] = (unsigned char)(n >> 8);  p[3] = (unsigned char)n;
}
static void put_be64(unsigned char *p, uint64_t n) {
    put_be32(p, (uint32_t)(n >> 32)); put_be32(p + 4, (uint32_t)n);
}
static void make_pkg(const char *p, int valid) {
    int f = open(p, O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(f >= 0);
    unsigned char h[0x1000] = {0};
    put_be32(h, valid ? 0x7f434e54 : 0x01020304);
    put_be32(h + 0x10, 1); /* table with one 32-byte entry */
    put_be32(h + 0x18, 0x1000);
    memcpy(h + 0x40, "UP0000-CUSA12345_00-0123456789ABCDEF", 36);
    put_be64(h + 0x410, 0x2000);
    put_be64(h + 0x418, 0x2000);
    put_be64(h + 0x430, 0x5000);
    assert(write(f, h, sizeof(h)) == sizeof(h));
    assert(ftruncate(f, 0x5000) == 0);
    close(f);
}
int main(void) {
    char base[] = "/tmp/ps4mount-disc-test-XXXXXX";
    assert(mkdtemp(base));
    char a[128], game[256], good[512], bad_dir[256], bad[512];
    snprintf(a, sizeof(a), "%s/app", base);
    assert(mkdir(a, 0700) == 0);
    snprintf(game, sizeof(game), "%s/CUSA12345", a);
    assert(mkdir(game, 0700) == 0);
    snprintf(good, sizeof(good), "%s/app.pkg", game);
    make_pkg(good, 1);
    disc_probe_result_t r;
    assert(disc_probe_scan(base, stdout, &r) == 0);
    assert(r.candidates == 1 && r.valid_headers == 1 && r.pfs_reads == 1 && r.errors == 0);
    snprintf(bad_dir, sizeof(bad_dir), "%s/CUSA99999", a);
    assert(mkdir(bad_dir, 0700) == 0);
    snprintf(bad, sizeof(bad), "%s/app.pkg", bad_dir);
    make_pkg(bad, 0);
    assert(disc_probe_scan(base, stdout, &r) == 0);
    assert(r.candidates == 2 && r.valid_headers == 1 && r.pfs_reads == 1 && r.errors == 1);
    assert(unlink(bad) == 0 && rmdir(bad_dir) == 0);
    assert(unlink(good) == 0 && rmdir(game) == 0 && rmdir(a) == 0 && rmdir(base) == 0);
    puts("disc_probe_test: PASS");
    return 0;
}
