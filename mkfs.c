#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "code/kernel/config/fs_img.h"
// #include "code/kernel/fs/sfs/sfs.h"
// #include "code/kernel/util/types.h"

// #include "config.h"
// #include "sfs.h"
// #include "types.h"

typedef unsigned char uchar;

#define __error(msg, quit, ...)                                                   \
    do {                                                                          \
        fprintf(stderr, #msg ":function %s - line %d: ", __FUNCTION__, __LINE__); \
        if (errno != 0) { fprintf(stderr, "[error] %s:", strerror(errno)); }      \
        fprintf(stderr, "\n\t"), fprintf(stderr, __VA_ARGS__);                    \
        errno = 0;                                                                \
        if (quit) { exit(-1); }                                                   \
    } while (0)

#define warn(...) __error(warn, 0, __VA_ARGS__)
#define bug(...) __error(bug, 1, __VA_ARGS__);

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = SFS_FSSIZE / (SFS_BSIZE * 8) + 1;
int ninodeblocks = SFS_NINODES / inodes_per_block + 1;
int nlog = SFS_LOGSIZE;
int nmeta;
int nblocks;

int fsfd;
Superblock sb;
char zeroes[SFS_BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void *);
void winode(uint, Dinode *);
void rinode(uint inum, Dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);

ushort xshort(ushort x) {
    ushort y;
    uchar *a = (uchar *)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

uint xint(uint x) {
    uint y;
    uchar *a = (uchar *)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

void rsect(uint sec, void *buf) {
    if (lseek(fsfd, sec * SFS_BSIZE, 0) != sec * SFS_BSIZE) { bug("lseek"); }
    if (read(fsfd, buf, SFS_BSIZE) != SFS_BSIZE) { bug("write"); }
}

void wsect(uint sec, void *buf) {
    if (lseek(fsfd, sec * SFS_BSIZE, 0) != sec * SFS_BSIZE) { bug("lseek"); }
    if (write(fsfd, buf, SFS_BSIZE) != SFS_BSIZE) { bug("write"); }
}

void winode(uint inum, Dinode *ip) {
    char buf[SFS_BSIZE];
    uint bn;
    Dinode *dip;
    bn = inum2block(inum, sb);
    rsect(bn, buf);
    dip = ((Dinode *)buf) + (inum % inodes_per_block);
    *dip = *ip;
    wsect(bn, buf);
}

void rinode(uint inum, Dinode *ip) {
    char buf[SFS_BSIZE];
    uint bn;
    Dinode *dip;
    bn = inum2block(inum, sb);
    rsect(bn, buf);
    dip = ((Dinode *)buf) + (inum % inodes_per_block);
    *ip = *dip;
}

uint ialloc(ushort type) {
    uint inum = freeinode++;
    Dinode din;
    memset(&din, 0, sizeof(din));
    din.type = xshort(type);
    din.nlink = xint(1);
    din.size = xint(0);
    winode(inum, &din);
    return inum;
}

void balloc(int used) {
    uchar buf[SFS_BSIZE];
    printf("balloc: first %d blocks have been allocated\n", used);
    assert(used < SFS_BLKBITS);
    memset(buf, 0, SFS_BSIZE);
    for (int i = 0; i < used; i++) { buf[i / 8] |= (0x1 << (i % 8)); }
    printf("balloc: write bitmap block at sector %d\n", sb.bmapstart);
    wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void iappend(uint inum, void *xp, int n) {
    char *p = (char *)xp;
    uint fbn, off, n1;
    Dinode din;
    char buf[SFS_BSIZE];
    uint indirect[SFS_NINDIRECT];
    uint x;
    rinode(inum, &din);
    off = xint(din.size);
    while (n > 0) {
        fbn = off / SFS_BSIZE;
        assert(fbn < SFS_MAXFILE_BLK);
        if (fbn < SFS_NDIRECT) {
            if (xint(din.addrs[fbn]) == 0) { din.addrs[fbn] = xint(freeblock++); }
            x = xint(din.addrs[fbn]);
        } else {
            if (xint(din.addrs[SFS_NDIRECT]) == 0) { din.addrs[SFS_NDIRECT] = xint(freeblock++); }
            rsect(xint(din.addrs[SFS_NDIRECT]), (char *)indirect);
            if (indirect[fbn - SFS_NDIRECT] == 0) {
                indirect[fbn - SFS_NDIRECT] = xint(freeblock++);
                wsect(xint(din.addrs[SFS_NDIRECT]), (char *)indirect);
            }
            x = xint(indirect[fbn - SFS_NDIRECT]);
        }
        n1 = min(n, (fbn + 1) * SFS_BSIZE - off);
        rsect(x, buf);
        memcpy(buf + off - (fbn * SFS_BSIZE), p, n1);
        wsect(x, buf);
        n -= n1;
        off += n1;
        p += n1;
    }
    din.size = xint(off);
    winode(inum, &din);
}

int main(int argc, char *argv[]) {
    int i, cc, fd;
    uint rootino, inum, off;
    Sfs_dirent de;
    char buf[SFS_BSIZE];
    Dinode din;
    assert(sizeof(int) == 4);
    if (argc < 2) { bug("Usage: mkfs disk0.img files...\n"); }
    assert((SFS_BSIZE % sizeof(Dinode)) == 0);
    assert((SFS_BSIZE % sizeof(Sfs_dirent)) == 0);
    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fsfd < 0) { bug("%s", argv[1]); }

    #define BUFFER_SIZE (4 * 1024 * 1024)  // 4MB
    char *buffer = calloc(1, BUFFER_SIZE);
    if (buffer == NULL) { bug("Failed to allocate buffer: %s", strerror(errno)); }

    off_t offset = 0;
    ssize_t bytes_to_write = SFS_DISKSIZE;

    while (bytes_to_write > 0) {
        ssize_t write_size = (bytes_to_write > BUFFER_SIZE) ? BUFFER_SIZE : bytes_to_write;
        ssize_t ret = pwrite(fsfd, buffer, write_size, offset);
        if (ret != write_size) { bug("Failed to write zeros: %s", strerror(errno)); }
        offset += write_size;
        bytes_to_write -= write_size;
    }
    free(buffer);

    nmeta = 2 + nlog + ninodeblocks + nbitmap;
    nblocks = SFS_FSSIZE - nmeta;
    sb.magic = SFS_MAGIC;
    sb.size = xint(SFS_FSSIZE);
    sb.nblocks = xint(nblocks);
    sb.ninodes = xint(SFS_NINODES);
    sb.nlog = xint(nlog);
    sb.logstart = xint(2);
    sb.inodestart = xint(2 + nlog);
    sb.bmapstart = xint(2 + nlog + ninodeblocks);
    printf("nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks %u) blocks %d total %d\n",
           nmeta, nlog, ninodeblocks, nbitmap, nblocks, SFS_FSSIZE);
    freeblock = nmeta;
    // for (i = 0; i < SFS_FSSIZE; i++) { wsect(i, zeroes); }
    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wsect(1, buf);
    rootino = ialloc(SFS_TYPE_DIR);
    assert(rootino == SFS_ROOTINO);

    memset(&de, 0, sizeof(de));
    de.inum = xint(rootino);
    strcpy(de.name, ".");
    iappend(rootino, &de, sizeof(de));

    memset(&de, 0, sizeof(de));
    de.inum = xint(rootino);
    strcpy(de.name, "..");
    iappend(rootino, &de, sizeof(de));
    for (i = 2; i < argc; i++) {
        int cmd_flag = 0;
        char *shortname = argv[i];
        if(strncmp(argv[i], "code/user/cmd/", 14) == 0){
            shortname = argv[i] + 5;
            cmd_flag = 1;
        }
        if ((fd = open(argv[i], 0)) < 0) { bug("%s", argv[i]); }
        // Skip leading _ in name when writing to file system.
        // The binaries are named _rm, _cat, etc. to keep the
        // build operating system from trying to execute them
        // in place of system binaries like rm and cat.
        if(shortname[0] == '_'){
            assert(cmd_flag == 1);
            shortname += 1;
        }
        inum = ialloc(SFS_TYPE_FILE);
        memset(&de, 0, sizeof(de));
        de.inum = xint(inum);
        assert(strlen(shortname) <= SFS_DIRSIZE);
        strncpy(de.name, shortname, strlen(shortname));
        iappend(rootino, &de, sizeof(de));
        while ((cc = read(fd, buf, sizeof(buf))) > 0) { iappend(inum, buf, cc); }
        close(fd);
    }
    rinode(rootino, &din);
    off = xint(din.size);
    off = ((off / SFS_BSIZE) + 1) * SFS_BSIZE;
    din.size = xint(off);
    winode(rootino, &din);
    balloc(freeblock);
    printf("disk0 has been set up!!!\n");
    exit(0);
}