#ifndef __CONFIG_FS_IMG_H__
#define __CONFIG_FS_IMG_H__

#include "config.h"

#define SFS_ROOTINO 1

#define SFS_BLKN_SUPER 1

#define SFS_NINDIRECT (SFS_BSIZE / sizeof(unsigned int))
#define SFS_MAXFILE_BLK (SFS_NDIRECT + SFS_NINDIRECT + SFS_NINDIRECT * SFS_NINDIRECT)
#define SFS_MAXFILE_SIZE (SFS_MAXFILE_BLK * SFS_BSIZE)

#define SFS_FSSIZE (SFS_DISKSIZE / SFS_BSIZE)  // size of file system in blocks

#define SFS_TYPE_INVAL 0
#define SFS_TYPE_FILE 1
#define SFS_TYPE_DIR 2
#define SFS_TYPE_LINK 3

typedef struct superblock Superblock;
struct superblock {
    unsigned int magic;       // magic number
    unsigned int size;        // Size of file system image (blocks)
    unsigned int nblocks;     // Number of data blocks
    unsigned int ninodes;     // Number of inodes.
    unsigned int nlog;        // Number of log blocks
    unsigned int logstart;    // Block number of first log block
    unsigned int inodestart;  // Block number of first inode block
    unsigned int bmapstart;   // Block number of first free map block
};

/* #define INODE_COMMON \
//     unsigned short type;  // File type    \
//     unsigned int nlink;  // Number of links to inode in file system \
//     unsigned int size;   // Size of file (bytes) \
    unsigned int addrs[SFS_NDIRECT + 1];  // Data block addresses */

typedef struct dinode Dinode;
struct dinode {
    unsigned short type;                    // File type
    unsigned int nlink;                   // Number of links to inode in file system
    unsigned int size;                    // Size of file (bytes)
    unsigned int addrs[SFS_NDIRECT + 2];  // Data block addresses
};


#define SFS_DIRSIZE (256 - 4 - 1)
typedef struct sfs_dirent Sfs_dirent;
struct sfs_dirent {
    unsigned int inum;
    char name[SFS_DIRSIZE + 1];
};

typedef struct dirent Dirent;
struct dirent{
    off_t offset;
    char name[SFS_DIRSIZE + 1];
};

// Inodes per block.
#define inodes_per_block (SFS_BSIZE / sizeof(Dinode))
// Block containing inode i
#define inum2block(i, sb) ((i) / inodes_per_block + sb.inodestart)
// Bitmap bits per block
#define SFS_BLKBITS (SFS_BSIZE * 8)
// Block of free map containing bit for block b
#define block2map_block(b, sb) ((b) / SFS_BLKBITS + sb.bmapstart)

#endif