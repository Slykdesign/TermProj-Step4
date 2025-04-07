#ifndef INODE_H
#define INODE_H

#include "ext2.h"

typedef struct {
    // Fields for inode structure...
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
} Inode;

// New function prototypes
int32_t fetchInode(Ext2File *f, uint32_t iNum, Inode *buf);
int32_t writeInode(Ext2File *f, uint32_t iNum, Inode *buf);
int32_t inodeInUse(Ext2File *f, uint32_t iNum);
uint32_t allocateInode(Ext2File *f, int32_t group);
int32_t freeInode(Ext2File *f, uint32_t iNum);
void displayInode(Inode *inode);

#endif //INODE_H