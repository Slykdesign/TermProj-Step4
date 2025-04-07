#include "ext2.h"
#include "inode.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int32_t fetchBlockFromFile(Ext2File *f, Inode *i, uint32_t bNum, void *buf) {
    uint32_t blockNum;
    if (bNum < 12) {
        // Direct block
        blockNum = i->i_block[bNum];
    } else {
        uint32_t k = f->block_size / 4;
        if (bNum < 12 + k) {
            // Single indirect block
            if (i->i_block[12] == 0) return -1;
            uint32_t singleIndirectBlock[k];
            if (!fetchBlock(f, i->i_block[12], singleIndirectBlock)) return -1;
            blockNum = singleIndirectBlock[bNum - 12];
        } else if (bNum < 12 + k + k * k) {
            // Double indirect block
            if (i->i_block[13] == 0) return -1;
            uint32_t doubleIndirectBlock[k];
            if (!fetchBlock(f, i->i_block[13], doubleIndirectBlock)) return -1;
            uint32_t singleIndirectBlock[k];
            if (!fetchBlock(f, doubleIndirectBlock[(bNum - 12 - k) / k], singleIndirectBlock)) return -1;
            blockNum = singleIndirectBlock[(bNum - 12 - k) % k];
        } else {
            // Triple indirect block
            if (i->i_block[14] == 0) return -1;
            uint32_t tripleIndirectBlock[k];
            if (!fetchBlock(f, i->i_block[14], tripleIndirectBlock)) return -1;
            uint32_t doubleIndirectBlock[k];
            if (!fetchBlock(f, tripleIndirectBlock[(bNum - 12 - k - k * k) / (k * k)], doubleIndirectBlock)) return -1;
            uint32_t singleIndirectBlock[k];
            if (!fetchBlock(f, doubleIndirectBlock[(bNum - 12 - k - k * k) % (k * k) / k], singleIndirectBlock)) return -1;
            blockNum = singleIndirectBlock[(bNum - 12 - k - k * k) % k];
        }
    }
    return fetchBlock(f, blockNum, buf) ? 0 : -1;
}

int32_t writeBlockToFile(Ext2File *f, Inode *i, uint32_t bNum, void *buf) {
    uint32_t blockNum;
    if (bNum < 12) {
        // Direct block
        blockNum = i->i_block[bNum];
    } else {
        uint32_t k = f->block_size / 4;
        if (bNum < 12 + k) {
            // Single indirect block
            if (i->i_block[12] == 0) return -1;
            uint32_t singleIndirectBlock[k];
            if (!fetchBlock(f, i->i_block[12], singleIndirectBlock)) return -1;
            blockNum = singleIndirectBlock[bNum - 12];
        } else if (bNum < 12 + k + k * k) {
            // Double indirect block
            if (i->i_block[13] == 0) return -1;
            uint32_t doubleIndirectBlock[k];
            if (!fetchBlock(f, i->i_block[13], doubleIndirectBlock)) return -1;
            uint32_t singleIndirectBlock[k];
            if (!fetchBlock(f, doubleIndirectBlock[(bNum - 12 - k) / k], singleIndirectBlock)) return -1;
            blockNum = singleIndirectBlock[(bNum - 12 - k) % k];
        } else {
            // Triple indirect block
            if (i->i_block[14] == 0) return -1;
            uint32_t tripleIndirectBlock[k];
            if (!fetchBlock(f, i->i_block[14], tripleIndirectBlock)) return -1;
            uint32_t doubleIndirectBlock[k];
            if (!fetchBlock(f, tripleIndirectBlock[(bNum - 12 - k - k * k) / (k * k)], doubleIndirectBlock)) return -1;
            uint32_t singleIndirectBlock[k];
            if (!fetchBlock(f, doubleIndirectBlock[(bNum - 12 - k - k * k) % (k * k) / k], singleIndirectBlock)) return -1;
            blockNum = singleIndirectBlock[(bNum - 12 - k - k * k) % k];
        }
    }
    return writeBlock(f, blockNum, buf) ? 0 : -1;
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