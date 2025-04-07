#include "ext2.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

Ext2File *openExt2File(const char *filename) {
    Ext2File *ext2 = malloc(sizeof(Ext2File));
    if (!ext2) return NULL;

    ext2->partition = openPartition(filename, 0);
    if (!ext2->partition) {
        free(ext2);
        return NULL;
    }

    // Read the main superblock located at an offset of 1024 bytes from the start of the partition
    if (!fetchSuperblock(ext2, 0, &ext2->superblock)) {
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    // Validate the superblock by checking the magic number
    if (ext2->superblock.s_magic != 0xEF53) {
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    // Calculate block size and number of block groups
    ext2->block_size = 1024 << ext2->superblock.s_log_block_size;
    ext2->num_block_groups = (ext2->superblock.s_blocks_count + ext2->superblock.s_blocks_per_group - 1) / ext2->superblock.s_blocks_per_group;

    // Allocate memory for block group descriptor table
    ext2->bgdt = malloc(ext2->num_block_groups * sizeof(Ext2BlockGroupDescriptor));
    if (!ext2->bgdt) {
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    // Read the block group descriptor table
    if (!fetchBGDT(ext2, 2, ext2->bgdt)) {
        closePartition(ext2->partition);
        free(ext2->bgdt);
        free(ext2);
        return NULL;
    }

    return ext2;
}

int32_t fetchInode(Ext2File *f, uint32_t iNum, Inode *buf) {
    if (iNum == 0 || iNum > f->superblock.s_inodes_count) {
        return -1; // Invalid inode number
    }
    iNum--; // Convert to 0-based index

    uint32_t group = iNum / f->superblock.s_inodes_per_group;
    uint32_t index = iNum % f->superblock.s_inodes_per_group;
    uint32_t block = f->bgdt[group].bg_inode_table + (index * f->superblock.s_inode_size) / f->block_size;
    uint32_t offset = (index * f->superblock.s_inode_size) % f->block_size;

    uint8_t block_buf[f->block_size];
    if (!fetchBlock(f, block, block_buf)) {
        return -1; // Error reading block
    }
    memcpy(buf, block_buf + offset, sizeof(Inode));
    return 0; // Success
}

int32_t writeInode(Ext2File *f, uint32_t iNum, Inode *buf) {
    if (iNum == 0 || iNum > f->superblock.s_inodes_count) {
        return -1; // Invalid inode number
    }
    iNum--; // Convert to 0-based index

    uint32_t group = iNum / f->superblock.s_inodes_per_group;
    uint32_t index = iNum % f->superblock.s_inodes_per_group;
    uint32_t block = f->bgdt[group].bg_inode_table + (index * f->superblock.s_inode_size) / f->block_size;
    uint32_t offset = (index * f->superblock.s_inode_size) % f->block_size;

    uint8_t block_buf[f->block_size];
    if (!fetchBlock(f, block, block_buf)) {
        return -1; // Error reading block
    }
    memcpy(block_buf + offset, buf, sizeof(Inode));
    if (!writeBlock(f, block, block_buf)) {
        return -1; // Error writing block
    }
    return 0; // Success
}

int32_t inodeInUse(Ext2File *f, uint32_t iNum) {
    if (iNum == 0 || iNum > f->superblock.s_inodes_count) {
        return -1; // Invalid inode number
    }
    iNum--; // Convert to 0-based index

    uint32_t group = iNum / f->superblock.s_inodes_per_group;
    uint32_t index = iNum % f->superblock.s_inodes_per_group;
    uint32_t block = f->bgdt[group].bg_inode_bitmap + (index / (8 * f->block_size));
    uint32_t offset = index % (8 * f->block_size);

    uint8_t block_buf[f->block_size];
    if (!fetchBlock(f, block, block_buf)) {
        return -1; // Error reading block
    }
    return (block_buf[offset / 8] & (1 << (offset % 8))) != 0;
}

uint32_t allocateInode(Ext2File *f, int32_t group) {
    for (uint32_t g = (group == -1 ? 0 : group); g < f->num_block_groups; g++) {
        uint8_t block_buf[f->block_size];
        if (!fetchBlock(f, f->bgdt[g].bg_inode_bitmap, block_buf)) {
            return 0; // Error reading block
        }
        for (uint32_t i = 0; i < f->block_size * 8; i++) {
            if ((block_buf[i / 8] & (1 << (i % 8))) == 0) {
                block_buf[i / 8] |= (1 << (i % 8));
                if (!writeBlock(f, f->bgdt[g].bg_inode_bitmap, block_buf)) {
                    return 0; // Error writing block
                }
                return g * f->superblock.s_inodes_per_group + i + 1; // Return 1-based inode number
            }
        }
        if (group != -1) {
            break; // Only search the specified group
        }
    }
    return 0; // No free inode found
}

int32_t freeInode(Ext2File *f, uint32_t iNum) {
    if (iNum == 0 || iNum > f->superblock.s_inodes_count) {
        return -1; // Invalid inode number
    }
    iNum--; // Convert to 0-based index

    uint32_t group = iNum / f->superblock.s_inodes_per_group;
    uint32_t index = iNum % f->superblock.s_inodes_per_group;
    uint32_t block = f->bgdt[group].bg_inode_bitmap + (index / (8 * f->block_size));
    uint32_t offset = index % (8 * f->block_size);

    uint8_t block_buf[f->block_size];
    if (!fetchBlock(f, block, block_buf)) {
        return -1; // Error reading block
    }
    block_buf[offset / 8] &= ~(1 << (offset % 8));
    if (!writeBlock(f, block, block_buf)) {
        return -1; // Error writing block
    }
    return 0; // Success
}

void displayInode(Inode *inode) {
    printf("Inode:\n");
    printf("  Mode: %u\n", inode->i_mode);
    printf("  User ID: %u\n", inode->i_uid);
    printf("  Size: %u\n", inode->i_size);
    printf("  Access Time: %u\n", inode->i_atime);
    printf("  Creation Time: %u\n", inode->i_ctime);
    printf("  Modification Time: %u\n", inode->i_mtime);
    printf("  Deletion Time: %u\n", inode->i_dtime);
    printf("  Group ID: %u\n", inode->i_gid);
    printf("  Links Count: %u\n", inode->i_links_count);
    printf("  Blocks: %u\n", inode->i_blocks);
    printf("  Flags: %u\n", inode->i_flags);
    for (int i = 0; i < 15; i++) {
        printf("  Block[%d]: %u\n", i, inode->i_block[i]);
    }
    printf("  Generation: %u\n", inode->i_generation);
    printf("  File ACL: %u\n", inode->i_file_acl);
    printf("  Directory ACL: %u\n", inode->i_dir_acl);
    printf("  Fragment Address: %u\n", inode->i_faddr);
}

void closeExt2File(Ext2File *f) {
    if (f) {
        closePartition(f->partition);
        free(f->bgdt);
        free(f);
    }
}

bool fetchBlock(Ext2File *f, uint32_t blockNum, void *buf) {
    off_t offset = (blockNum + f->superblock.s_first_data_block) * f->block_size;
    if (vdiSeekPartition(f->partition, offset, SEEK_SET) < 0) return false;
    return vdiReadPartition(f->partition, buf, f->block_size) == f->block_size;
}

bool writeBlock(Ext2File *f, uint32_t blockNum, void *buf) {
    off_t offset = (blockNum + f->superblock.s_first_data_block) * f->block_size;
    if (vdiSeekPartition(f->partition, offset, SEEK_SET) < 0) return false;
    return writePartition(f->partition, buf, f->block_size) == f->block_size;
}

bool fetchSuperblock(Ext2File *f, uint32_t blockNum, Ext2Superblock *sb) {
    if (!fetchBlock(f, blockNum, sb)) return false;
    return sb->s_magic == 0xEF53;
}

bool writeSuperblock(Ext2File *f, uint32_t blockNum, Ext2Superblock *sb) {
    return writeBlock(f, blockNum, sb);
}

bool fetchBGDT(Ext2File *f, uint32_t blockNum, Ext2BlockGroupDescriptor *bgdt) {
    return fetchBlock(f, blockNum, bgdt);
}

bool writeBGDT(Ext2File *f, uint32_t blockNum, Ext2BlockGroupDescriptor *bgdt) {
    return writeBlock(f, blockNum, bgdt);
}

void displaySuperblock(Ext2Superblock *sb) {
    printf("Superblock:\n");
    printf("  Inodes count: %u\n", sb->s_inodes_count);
    printf("  Blocks count: %u\n", sb->s_blocks_count);
    printf("  Reserved blocks count: %u\n", sb->s_r_blocks_count);
    printf("  Free blocks count: %u\n", sb->s_free_blocks_count);
    printf("  Free inodes count: %u\n", sb->s_free_inodes_count);
    printf("  First data block: %u\n", sb->s_first_data_block);
    printf("  Block size: %u\n", 1024 << sb->s_log_block_size);
    printf("  Blocks per group: %u\n", sb->s_blocks_per_group);
    printf("  Inodes per group: %u\n", sb->s_inodes_per_group);
    printf("  Magic number: 0x%x\n", sb->s_magic);
}

void displayBGDT(Ext2BlockGroupDescriptor *bgdt, uint32_t num_block_groups) {
    printf("Block Group Descriptor Table:\n");
    for (uint32_t i = 0; i < num_block_groups; i++) {
        printf("  Block group %u:\n", i);
        printf("    Block bitmap: %u\n", bgdt[i].bg_block_bitmap);
        printf("    Inode bitmap: %u\n", bgdt[i].bg_inode_bitmap);
        printf("    Inode table: %u\n", bgdt[i].bg_inode_table);
        printf("    Free blocks count: %u\n", bgdt[i].bg_free_blocks_count);
        printf("    Free inodes count: %u\n", bgdt[i].bg_free_inodes_count);
        printf("    Used directories count: %u\n", bgdt[i].bg_used_dirs_count);
    }
}