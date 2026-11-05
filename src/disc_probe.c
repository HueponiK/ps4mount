/* SPDX-License-Identifier: GPL-3.0-or-later
 * Read-only feasibility probe for optical PS4 PKG files, optional in PS4Mount.
 * Does NOT install, mount, register, decrypt, or bypass any license checks.
 */
#define _POSIX_C_SOURCE 200809L
#include "disc_probe.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define PROBE_SIZE 4096u
#define PKG_MAGIC 0x7f434e54u
#define MAX_DISC_PACKAGES 32u

static uint32_t be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static uint64_t be64(const unsigned char *p) {
    return ((uint64_t)be32(p) << 32) | be32(p + 4);
}
static int read_at(int fd, void *dest, size_t count, uint64_t off) {
    unsigned char *p = dest;
    while (count) {
        /* Package lengths are checked against st_size before seeking. */
        ssize_t n = pread(fd, p, count, (off_t)off);
        if (n <= 0)
            return -1;
        p += n;
        off += (uint64_t)n;
        count -= (size_t)n;
    }
    return 0;
}
static int title_id(const char *s) {
    if (strlen(s) != 9 || memcmp(s, "CUSA", 4))
        return 0;
    for (int i = 4; i < 9; i++)
        if (s[i] < '0' || s[i] > '9')
            return 0;
    return 1;
}
static void safe_content_id(char out[37], const unsigned char *raw) {
    for (int i = 0; i < 36; i++) {
        unsigned char c = raw[i];
        out[i] = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                         (c >= '0' && c <= '9') || c == '-' || c == '_'
                     ? (char)c : '?';
    }
    out[36] = '\0';
}
static int inspect(const char *path, const char *tid, FILE *log, disc_probe_result_t *r) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(log, "[disc-probe] %s open failed: %s\n", tid, strerror(errno));
        r->errors++;
        return -1;
    }
    struct stat st;
    unsigned char h[0x1000];
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < (off_t)sizeof(h) ||
        read_at(fd, h, sizeof(h), 0)) {
        fprintf(log, "[disc-probe] %s cannot read 4 KiB header\n", tid);
        r->errors++;
        close(fd);
        return -1;
    }
    uint64_t actual_size = (uint64_t)st.st_size;
    uint32_t magic = be32(h), entries = be32(h + 0x10), table = be32(h + 0x18);
    uint64_t pfs_off = be64(h + 0x410), pfs_size = be64(h + 0x418);
    uint64_t declared_size = be64(h + 0x430);
    if (magic != PKG_MAGIC || entries > 512 || entries == 0 ||
        (uint64_t)table > actual_size || (uint64_t)entries * 32 > actual_size - table ||
        !declared_size || declared_size > actual_size || pfs_off > actual_size ||
        pfs_size > actual_size - pfs_off) {
        fprintf(log, "[disc-probe] %s invalid PKG header / impossible bounds "
                     "(magic=%08x, entries=%u)\n", tid, magic, entries);
        r->errors++;
        close(fd);
        return -1;
    }
    r->valid_headers++;
    char cid[37];
    safe_content_id(cid, h + 0x40);
    fprintf(log, "[disc-probe] %s header OK; content=%s; file=%" PRIu64
                 " bytes; declared=%" PRIu64 "; PFS offset=%" PRIu64 "; PFS size=%" PRIu64 "\n",
            tid, cid, actual_size, declared_size, pfs_off, pfs_size);
    /* Exactly two bounded 4 KiB reads from the encrypted PFS region. No dump or copy. */
    if (pfs_size < PROBE_SIZE) {
        fprintf(log, "[disc-probe] %s no full PFS block available for seek test\n", tid);
        r->errors++;
        close(fd);
        return -1;
    }
    unsigned char block[PROBE_SIZE];
    uint64_t last = pfs_off + pfs_size - PROBE_SIZE;
    if (read_at(fd, block, sizeof(block), pfs_off) ||
        read_at(fd, block, sizeof(block), last)) {
        fprintf(log, "[disc-probe] %s failed direct PFS read(s)\n", tid);
        r->errors++;
        close(fd);
        return -1;
    }
    r->pfs_reads++;
    fprintf(log, "[disc-probe] %s direct offset reads OK at %" PRIu64 " and %" PRIu64
                 "; package remains on source medium\n", tid, pfs_off, last);
    close(fd);
    return 0;
}

int disc_probe_scan(const char *root, FILE *log, disc_probe_result_t *out) {
    disc_probe_result_t local = {0};
    disc_probe_result_t *r = out ? out : &local;
    memset(r, 0, sizeof(*r));
    if (!root || !log) return -1;
    char app[PATH_MAX];
    int n = snprintf(app, sizeof(app), "%s/app", root);
    if (n < 0 || (size_t)n >= sizeof(app)) return -1;
    DIR *d = opendir(app);
    if (!d) {
        fprintf(log, "[disc-probe] cannot list %s: %s (disc absent or mounted elsewhere?)\n",
                app, strerror(errno));
        return -1;
    }
    fprintf(log, "[disc-probe] scanning %s (read-only)\n", app);
    struct dirent *de;
    while ((de = readdir(d)) != NULL && r->candidates < MAX_DISC_PACKAGES) {
        if (!title_id(de->d_name)) continue;
        char path[PATH_MAX];
        n = snprintf(path, sizeof(path), "%s/%s/app.pkg", app, de->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) continue;
        struct stat st;
        if (stat(path, &st) || !S_ISREG(st.st_mode)) continue;
        r->candidates++;
        inspect(path, de->d_name, log, r);
    }
    closedir(d);
    fprintf(log, "[disc-probe] summary: candidates=%u valid_headers=%u "
                 "pfs_reads=%u errors=%u\n",
            r->candidates, r->valid_headers, r->pfs_reads, r->errors);
    return 0;
}
