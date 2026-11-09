/*
 * ips3608flash — write FNIRSI IPS3608 firmware to the bootloader disk on macOS.
 * No dependencies (no Python, no mtools).
 *
 *   sudo ips3608flash APP_IPS3608_Vx.y_YYMMDD.bin            finds the device itself
 *   sudo ips3608flash --dry-run APP_IPS3608_....bin           check only
 *   ips3608flash --image disk.img APP_IPS3608_....bin         test on an image file
 *
 * Steps: find the single "Msc iap" disk, check 2048-byte blocks / 8386560 bytes,
 * read the whole FAT12 disk into memory, add the firmware file to a copy (long
 * file name + 8.3 entry, cluster chain in every FAT; same result as mtools'
 * mcopy), write back only the 2048-byte sectors that changed, then read the
 * device back and compare.
 *
 * Exit codes: 0 done, 1 error before writing (nothing written),
 *             2 write/verify problem (do not press RUN/STOP).
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DISK_SIZE   8386560u
#define DEV_BLOCK   2048u
#define MAX_CHANGED 512u

static void die(int code, const char *fmt, ...) __attribute__((format(printf, 2, 3), noreturn));
#include <stdarg.h>
static void die(int code, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fputs("ERROR: ", stdout); vprintf(fmt, ap); fputc('\n', stdout);
    va_end(ap); fflush(stdout); exit(code);
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static void wr16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void wr32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* ---------- FAT12 ---------- */
typedef struct {
    uint8_t *img; uint32_t size;
    uint32_t bps, spc, rsvd, nfats, rootents, totsec, fatsz;
    uint32_t root_off, data_off, clus_bytes, nclus;
} fat_t;

static void fat_open(fat_t *f, uint8_t *img, uint32_t size) {
    const uint8_t *b = img;
    f->img = img; f->size = size;
    f->bps = rd16(b + 11); f->spc = b[13]; f->rsvd = rd16(b + 14); f->nfats = b[16];
    f->rootents = rd16(b + 17); f->totsec = rd16(b + 19) ? rd16(b + 19) : rd32(b + 32);
    f->fatsz = rd16(b + 22);
    if (!(f->bps == 512 || f->bps == 1024 || f->bps == 2048 || f->bps == 4096) ||
        !f->spc || (f->spc & (f->spc - 1)) || !f->rsvd || !f->nfats || f->nfats > 4 ||
        !f->rootents || !f->fatsz || (uint64_t)f->totsec * f->bps > size)
        die(1, "disk does not contain a valid FAT boot sector.");
    uint32_t rootsecs = (f->rootents * 32 + f->bps - 1) / f->bps;
    uint32_t first_data = f->rsvd + f->nfats * f->fatsz + rootsecs;
    f->root_off = (f->rsvd + f->nfats * f->fatsz) * f->bps;
    f->data_off = first_data * f->bps;
    f->clus_bytes = f->bps * f->spc;
    f->nclus = (f->totsec - first_data) / f->spc;
    if (f->nclus >= 4085) die(1, "filesystem is not FAT12 (%u clusters).", f->nclus);
    if ((uint64_t)f->fatsz * f->bps * 2 < (uint64_t)(f->nclus + 2) * 3) die(1, "FAT too small.");
}

static uint32_t fat_get(fat_t *f, uint32_t n) {
    const uint8_t *p = f->img + f->rsvd * f->bps + n * 3 / 2;
    uint16_t v = rd16(p);
    return (n & 1) ? v >> 4 : v & 0xFFF;
}
static void fat_set(fat_t *f, uint32_t n, uint32_t val) {
    for (uint32_t k = 0; k < f->nfats; k++) {
        uint8_t *p = f->img + (f->rsvd + k * f->fatsz) * f->bps + n * 3 / 2;
        uint16_t v = rd16(p);
        v = (n & 1) ? (uint16_t)((v & 0x000F) | (val << 4)) : (uint16_t)((v & 0xF000) | (val & 0xFFF));
        wr16(p, v);
    }
}

static uint8_t lfn_sum(const uint8_t *sn) {
    uint8_t s = 0;
    for (int i = 0; i < 11; i++) s = (uint8_t)(((s & 1) << 7) + (s >> 1) + sn[i]);
    return s;
}

/* Remove existing *.BIN entries (and their LFN entries) and free their clusters. */
static void fat_remove_bins(fat_t *f) {
    uint8_t *root = f->img + f->root_off;
    for (uint32_t i = 0; i < f->rootents; i++) {
        uint8_t *e = root + i * 32;
        if (e[0] == 0x00) break;
        if (e[0] == 0xE5 || e[11] == 0x0F || (e[11] & 0x18)) continue;
        if (memcmp(e + 8, "BIN", 3) != 0) continue;
        uint32_t c = rd16(e + 26), guard = 0;
        while (c >= 2 && c < f->nclus + 2 && guard++ <= f->nclus) {
            uint32_t next = fat_get(f, c);
            fat_set(f, c, 0);
            c = next;
        }
        e[0] = 0xE5;
        for (uint32_t j = i; j-- > 0;) {
            uint8_t *l = root + j * 32;
            if (l[0] == 0xE5 || l[11] != 0x0F) break;
            l[0] = 0xE5;
        }
    }
}

static void fat_add_file(fat_t *f, const char *name, const uint8_t *data, uint32_t len) {
    /* 8.3 short name like mtools: first 6 valid chars of base + "~1", extension */
    uint8_t sn[11]; memset(sn, ' ', 11);
    const char *dot = strrchr(name, '.');
    int k = 0;
    for (const char *p = name; p < (dot ? dot : name + strlen(name)) && k < 6; p++) {
        unsigned char c = (unsigned char)toupper((unsigned char)*p);
        if (isalnum(c) || strchr("$%'-_@~`!(){}^#&", c)) sn[k++] = c;
    }
    sn[k] = '~'; sn[k + 1] = '1';
    if (dot) for (int i = 0; i < 3 && dot[1 + i]; i++) sn[8 + i] = (uint8_t)toupper((unsigned char)dot[1 + i]);
    uint8_t sum = lfn_sum(sn);

    size_t nlen = strlen(name);
    if (nlen > 255) die(1, "file name too long.");
    uint32_t nlfn = (uint32_t)(nlen + 12) / 13, need = nlfn + 1;

    /* find consecutive free root entries */
    uint8_t *root = f->img + f->root_off;
    uint32_t start = UINT32_MAX, run = 0;
    for (uint32_t i = 0; i < f->rootents; i++) {
        uint8_t c = root[i * 32];
        if (c == 0x00 || c == 0xE5) { if (!run++) start = i; if (run == need) break; }
        else run = 0;
    }
    if (run < need) die(1, "no free directory entries.");

    /* allocate clusters */
    uint32_t nc = (len + f->clus_bytes - 1) / f->clus_bytes, first = 0, prev = 0, got = 0;
    for (uint32_t c = 2; c < f->nclus + 2 && got < nc; c++) {
        if (fat_get(f, c) != 0) continue;
        if (prev) fat_set(f, prev, c); else first = c;
        prev = c; got++;
        uint32_t off = (got - 1) * f->clus_bytes, n = len - off < f->clus_bytes ? len - off : f->clus_bytes;
        uint8_t *dst = f->img + f->data_off + (c - 2) * f->clus_bytes;
        memset(dst, 0, f->clus_bytes);
        memcpy(dst, data + off, n);
    }
    if (got < nc) die(1, "not enough free space on the disk.");
    if (prev) fat_set(f, prev, 0xFFF);

    /* LFN entries, highest order first */
    for (uint32_t i = 0; i < nlfn; i++) {
        uint32_t ord = nlfn - i;
        uint8_t *e = root + (start + i) * 32;
        memset(e, 0, 32);
        e[0] = (uint8_t)(ord | (i == 0 ? 0x40 : 0)); e[11] = 0x0F; e[13] = sum;
        static const int pos[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
        for (int j = 0; j < 13; j++) {
            size_t ci = (ord - 1) * 13 + j;
            uint16_t ch = ci < nlen ? (uint8_t)name[ci] : ci == nlen ? 0x0000 : 0xFFFF;
            wr16(e + pos[j], ch);
        }
    }
    /* short entry */
    uint8_t *e = root + (start + nlfn) * 32;
    memset(e, 0, 32);
    memcpy(e, sn, 11); e[11] = 0x20;
    time_t now = time(NULL); struct tm t; localtime_r(&now, &t);
    uint16_t dt = (uint16_t)(((t.tm_year - 80) << 9) | ((t.tm_mon + 1) << 5) | t.tm_mday);
    uint16_t tm = (uint16_t)((t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec / 2));
    wr16(e + 14, tm); wr16(e + 16, dt); wr16(e + 18, dt); wr16(e + 22, tm); wr16(e + 24, dt);
    wr16(e + 26, (uint16_t)first); wr32(e + 28, len);
}

/* ---------- I/O ---------- */
static uint8_t *read_all(const char *path, uint32_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;
    uint8_t *buf = malloc(size);
    uint32_t got = 0;
    while (got < size) {
        ssize_t n = pread(fd, buf + got, size - got, got);
        if (n <= 0) { free(buf); close(fd); return NULL; }
        got += (uint32_t)n;
    }
    close(fd);
    return buf;
}

/* Find the single disk whose media name is "Msc iap"; returns "diskN". */
static void find_disk(char *out, size_t outsz) {
    FILE *p = popen("diskutil list external physical", "r");
    if (!p) die(1, "cannot run diskutil.");
    char line[512], disks[16][32]; int n = 0, hits = 0;
    while (fgets(line, sizeof line, p) && n < 16)
        if (!strncmp(line, "/dev/disk", 9)) sscanf(line + 5, "%31[a-z0-9]", disks[n++]);
    pclose(p);
    for (int i = 0; i < n; i++) {
        char cmd[128]; snprintf(cmd, sizeof cmd, "diskutil info %s", disks[i]);
        FILE *q = popen(cmd, "r");
        if (!q) continue;
        int match = 0;
        while (fgets(line, sizeof line, q))
            if (strstr(line, "Media Name:") && strstr(line, "Msc iap")) match = 1;
        pclose(q);
        if (match) { if (!hits++) snprintf(out, outsz, "%s", disks[i]); }
    }
    if (hits > 1) die(1, "more than one IPS3608 in upgrade mode is connected; leave only one.");
    if (!hits)
        die(1, "IPS3608 not found.\n"
               "  1. Turn the power supply off.\n"
               "  2. Hold the UPPER button and turn the power on (Firmware Upgrade screen).\n"
               "  3. Connect the REAR USB port with the cable FROM THE BOX.");
}

int main(int argc, char **argv) {
    int dry = 0, image = 0, yes = 0;
    const char *pos[2]; int npos = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dry-run")) dry = 1;
        else if (!strcmp(argv[i], "--image")) image = 1;
        else if (!strcmp(argv[i], "--yes") || !strcmp(argv[i], "-y")) yes = 1;
        else if (argv[i][0] == '-') die(1, "unknown option %s", argv[i]);
        else if (npos < 2) pos[npos++] = argv[i];
        else npos = 3;
    }
    if (npos < 1 || npos > 2 || (image && npos != 2)) {
        fprintf(stderr, "usage: sudo %s APP_IPS3608_*.bin              (finds the IPS3608 itself)\n"
                        "       sudo %s --dry-run APP_IPS3608_*.bin    (check only, write nothing)\n"
                        "       %s --image disk.img APP_IPS3608_*.bin  (test on an image)\n",
                argv[0], argv[0], argv[0]);
        return 1;
    }
    const char *fwpath = pos[npos - 1];
    const char *fwname = strrchr(fwpath, '/'); fwname = fwname ? fwname + 1 : fwpath;
    size_t fl = strlen(fwname);
    if (strncmp(fwname, "APP_IPS3608_", 12) || fl < 16 || strcasecmp(fwname + fl - 4, ".bin"))
        die(1, "firmware file name must look like APP_IPS3608_*.bin (got %s).", fwname);

    struct stat st;
    if (stat(fwpath, &st) || !S_ISREG(st.st_mode)) die(1, "cannot open firmware %s.", fwpath);
    if (st.st_size < 1024 || st.st_size > 4 * 1024 * 1024) die(1, "firmware size %lld looks wrong.", (long long)st.st_size);
    uint32_t fwlen = (uint32_t)st.st_size;
    uint8_t *fw = read_all(fwpath, fwlen);
    if (!fw) die(1, "cannot read firmware %s: %s", fwpath, strerror(errno));

    char devbuf[64]; const char *dev;
    if (npos == 2) dev = pos[0];
    else {
        if (geteuid() != 0) die(1, "run with sudo:  sudo %s %s", argv[0], fwpath);
        char disk[32]; find_disk(disk, sizeof disk);
        snprintf(devbuf, sizeof devbuf, "/dev/r%s", disk); dev = devbuf;
        printf("Found IPS3608: /dev/%s\n", disk);
        char cmd[96]; snprintf(cmd, sizeof cmd, "diskutil unmountDisk force /dev/%s >/dev/null", disk);
        if (system(cmd) != 0) die(1, "cannot unmount /dev/%s.", disk);
    }

    /* device geometry */
    if (!image) {
        if (strncmp(dev, "/dev/rdisk", 10)) die(1, "device must be /dev/rdiskN.");
        int fd = open(dev, O_RDONLY);
        if (fd < 0) die(1, "cannot open %s: %s", dev, strerror(errno));
        uint32_t bs = 0; uint64_t cnt = 0;
        if (ioctl(fd, DKIOCGETBLOCKSIZE, &bs) || ioctl(fd, DKIOCGETBLOCKCOUNT, &cnt)) die(1, "cannot query %s.", dev);
        close(fd);
        if (bs != DEV_BLOCK || cnt * bs != DISK_SIZE)
            die(1, "%s is not an IPS3608 bootloader disk (block %u, size %llu).", dev, bs, (unsigned long long)(cnt * bs));
    } else {
        if (stat(dev, &st) || st.st_size != DISK_SIZE) die(1, "image must be exactly %u bytes.", DISK_SIZE);
    }

    uint8_t *before = read_all(dev, DISK_SIZE);
    if (!before) die(1, "cannot read %s: %s", dev, strerror(errno));
    uint8_t *after = malloc(DISK_SIZE);
    memcpy(after, before, DISK_SIZE);

    fat_t f; fat_open(&f, after, DISK_SIZE);
    fat_remove_bins(&f);
    fat_add_file(&f, fwname, fw, fwlen);

    uint32_t nsec = DISK_SIZE / DEV_BLOCK, nch = 0;
    uint32_t *ch = malloc(nsec * sizeof *ch);
    for (uint32_t s = 0; s < nsec; s++)
        if (memcmp(before + s * DEV_BLOCK, after + s * DEV_BLOCK, DEV_BLOCK)) ch[nch++] = s;
    printf("Firmware: %s (%u bytes)\nChanged sectors: %u\n", fwname, fwlen, nch);
    if (!nch) die(1, "this firmware is already on the device. Power-cycle into upgrade mode and retry.");
    if (nch > MAX_CHANGED) die(1, "%u changed sectors exceeds safety limit %u.", nch, MAX_CHANGED);

    if (dry) {
        if (image) {
            char out[4096]; snprintf(out, sizeof out, "%s.after", dev);
            int o = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (o < 0 || write(o, after, DISK_SIZE) != (ssize_t)DISK_SIZE) die(1, "cannot write %s", out);
            close(o); printf("Prepared image: %s\n", out);
        }
        puts("Dry run: nothing written.");
        return 0;
    }

    if (!yes && !image) {
        printf("Flash %s? [y/N] ", fwname); fflush(stdout);
        char ans[16] = "";
        if (!fgets(ans, sizeof ans, stdin) || (ans[0] != 'y' && ans[0] != 'Y')) die(1, "cancelled.");
    }

    int fd = open(dev, O_RDWR | (image ? 0 : O_SYNC));
    if (fd < 0) die(1, "cannot open %s for writing: %s", dev, strerror(errno));
    for (uint32_t i = 0; i < nch; i++) {
        off_t off = (off_t)ch[i] * DEV_BLOCK;
        if (pwrite(fd, after + off, DEV_BLOCK, off) != (ssize_t)DEV_BLOCK)
            die(2, "write failed at sector %u: %s. DO NOT PRESS RUN/STOP.", ch[i], strerror(errno));
    }
    fsync(fd); close(fd);
    printf("Wrote %u sectors.\n", nch); fflush(stdout);

    if (!image) sleep(1);
    uint8_t *check = read_all(dev, DISK_SIZE);
    if (!check) {
        if (access(dev, F_OK) != 0) {
            puts("\nDONE. The power supply took the firmware and is updating by itself.\n"
                 "Wait for it to reboot, then check the version in its settings.\n"
                 "Do not disconnect USB or power until it reboots.");
            return 0;
        }
        die(2, "cannot read the device back. DO NOT PRESS RUN/STOP.");
    }
    if (memcmp(check, after, DISK_SIZE)) die(2, "verification failed. DO NOT PRESS RUN/STOP.");
    puts("\nDONE. Firmware written and verified.\n"
         "Now SHORT-PRESS RUN/STOP on the power supply to start the update.\n"
         "Do not disconnect USB or power until it reboots.");
    return 0;
}
