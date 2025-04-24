#include "ext2.h"
#include "partition.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// Define the superblock offset for ext2 (always at 1024 bytes)
#define EXT2_SUPERBLOCK_OFFSET 1024
#define EXT2_SUPER_MAGIC 0xEF53

struct Ext2File *openExt2(char *fn) {
    // Allocate memory for Ext2File
    struct Ext2File *ext2 = malloc(sizeof(struct Ext2File));
    if (!ext2) {
        printf("Failed to allocate memory for Ext2File.\n");
        return NULL;
    }

    // Open the MBR partition
    ext2->partition = openPartition(fn, 0);
    if (!ext2->partition) {
        printf("Failed to open MBR partition.\n");
        free(ext2);
        return NULL;
    }

    // Find the partition of type 0x83
    int partIndex = 1;
    for (int i = 0; i < 4; ++i) {
        MBRPartitionEntry *entry = (MBRPartitionEntry *)(ext2->partition->partitionTable + i * 16);
        if (entry->partitionType == 0x83) {  // Check for Linux ext2 partition type
            partIndex = i;
            break;
        }
    }

    if (partIndex < 0) {
        printf("No ext2 partition of type 0x83 found.\n");
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    // Close and reopen the partition for the identified partition index
    closePartition(ext2->partition);
    ext2->partition = openPartition(fn, partIndex);
    if (!ext2->partition) {
        printf("Failed to open partition %d.\n", partIndex);
        free(ext2);
        return NULL;
    }

    // Calculate the superblock offset
    off_t superblockOffset = ext2->partition->startSector * 512 + EXT2_SUPERBLOCK_OFFSET;

    // Attempt to seek to the superblock
    if (vdiSeekPartition(ext2->partition, superblockOffset, SEEK_SET) != superblockOffset) {
        printf("Failed to seek to superblock in partition %d.\n", partIndex);
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    // Read the superblock
    if (!fetchSuperblock(ext2, 0, &ext2->superblock)) {
        printf("Failed to fetch the superblock in partition %d.\n", partIndex);
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    // Validate the superblock magic number
    if (ext2->superblock.s_magic != EXT2_SUPER_MAGIC) {
        printf("Invalid superblock magic number (0x%X).\n", ext2->superblock.s_magic);
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    // Calculate block size and number of block groups
    ext2->blockSize = 1024 << ext2->superblock.s_log_block_size;
    ext2->numBlockGroups = (ext2->superblock.s_blocks_count + ext2->superblock.s_blocks_per_group - 1) / ext2->superblock.s_blocks_per_group;

    // Allocate memory for block group descriptor table
    ext2->bgdt = malloc(ext2->numBlockGroups * sizeof(Ext2BlockGroupDescriptor));
    if (!ext2->bgdt) {
        printf("Failed to allocate memory for block group descriptor table.\n");
        closePartition(ext2->partition);
        free(ext2);
        return NULL;
    }

    // Fetch the block group descriptor table
    uint32_t bgdt_block = (ext2->blockSize == 1024) ? 2 : 1;
    if (!fetchBGDT(ext2, bgdt_block, ext2->bgdt)) {
        printf("Failed to fetch block group descriptor table.\n");
        closePartition(ext2->partition);
        free(ext2->bgdt);
        free(ext2);
        return NULL;
    }

    // Successfully opened the ext2 file system
    return ext2;
}

void closeExt2(struct Ext2File *f) {
    if (f) {
        closePartition(f->partition);
        free(f->bgdt);
        free(f);
    }
}

bool fetchBlock(struct Ext2File *f, uint32_t blockNum, void *buf) {
    uint64_t offset = (blockNum + f->superblock.s_first_data_block) * f->blockSize;
    if (vdiSeekPartition(f->partition, offset, SEEK_SET) != offset) {
        printf("Failed to seek to block %u\n", blockNum);
        return false;
    }
    if (vdiReadPartition(f->partition, buf, f->blockSize) != f->blockSize) {
        printf("Failed to read block %u\n", blockNum);
        return false;
    }
    return true;
}

bool writeBlock(struct Ext2File *f, uint32_t blockNum, void *buf) {
    off_t offset = (blockNum + f->superblock.s_first_data_block) * f->blockSize;
    if (vdiSeekPartition(f->partition, offset, SEEK_SET) != offset) {
        printf("Failed to seek to block %u for writing\n", blockNum);
        return false;
    }
    if (writePartition(f->partition, buf, f->blockSize) != f->blockSize) {
        printf("Failed to write block %u\n", blockNum);
        return false;
    }
    return true;
}

bool fetchSuperblock(struct Ext2File *f, uint32_t blockNum, Ext2Superblock *sb) {
    if (blockNum == 0) {
        if (vdiSeekPartition(f->partition, EXT2_SUPERBLOCK_OFFSET, SEEK_SET) != EXT2_SUPERBLOCK_OFFSET) {
            printf("Failed to seek main superblock offset\n");
            return false;
        }
        if (vdiReadPartition(f->partition, sb, sizeof(Ext2Superblock)) != sizeof(Ext2Superblock)) {
            printf("Failed to read main superblock\n");
            return false;
        }
    } else {
        uint32_t blockSize = 1024 << sb->s_log_block_size;
        uint8_t *buf = (uint8_t *)malloc(blockSize);

        if (blockSize == 1024 && blockNum > 0) {
            blockNum++;
        }

        printf("%d\n", blockNum);
        if (!fetchSuperblock(f, blockNum, buf)) {
            printf("Failed to fetch backup superblock\n");
            free(buf);
            return false;
        }

        memcpy(sb, buf, sizeof(Ext2Superblock));
        free(buf);
    }

    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        printf("Invalid superblock 0x%X\n", sb->s_magic);
        return false;
    }
    return true;
}

bool writeSuperblock(struct Ext2File *f, uint32_t blockNum, Ext2Superblock *sb) {
    if (blockNum == 0) {
        // Write the main superblock using partition-level functions.
        if (vdiSeekPartition(f->partition, EXT2_SUPERBLOCK_OFFSET, SEEK_SET) != EXT2_SUPERBLOCK_OFFSET) {
            printf("Failed to seek to main superblock offset for writing\n");
            return false;
        }
        if (writePartition(f->partition, sb, sizeof(Ext2Superblock)) != sizeof(Ext2Superblock)) {
            printf("Failed to write main superblock\n");
            return false;
        }
    } else {
        uint32_t blockSize = f->blockSize; // Derived from superblock info.
        // Adjust blockNum for 1KB block file systems.
        if (blockSize == 1024 && blockNum > 0) {
            blockNum++;
        }
        // Allocate a temporary buffer, write the superblock into it.
        uint8_t *buf = (uint8_t *)malloc(blockSize * sizeof(uint8_t));
        memcpy(buf, sb, sizeof(Ext2Superblock));

        // Use writeBlock() to write the backup superblock.
        if (!writeBlock(f, blockNum, buf)) {
            printf("Failed to write backup superblock\n");
            free(buf);
            return false;
        }

        // For verification, read the block back.
        uint8_t *verifyBuf = (uint8_t *)malloc(blockSize * sizeof(uint8_t));
        if (!fetchBlock(f, blockNum, verifyBuf)) {
            printf("Failed to read back backup superblock for verification\n");
            free(buf);
            free(verifyBuf);
            return false;
        }

        Ext2Superblock verifySB;
        memcpy(&verifySB, verifyBuf, sizeof(Ext2Superblock));
        free(verifyBuf);

        if (verifySB.s_magic != EXT2_SUPER_MAGIC) {
            printf("Invalid backup superblock after writing: 0x%x\n", verifySB.s_magic);
            free(buf);
            return false;
        }
        free(buf);
    }
    return true;
}

bool fetchBGDT(struct Ext2File *f, uint32_t blockNum, Ext2BlockGroupDescriptor *bgdt) {
    uint8_t *buffer = malloc(f->blockSize);
    if (!buffer) return false;

    if (!fetchBlock(f, blockNum, bgdt)) {
        printf("Failed to fetch Block Group Descriptor Table at block %u\n", blockNum);
        return false;
    }

    memcpy(bgdt, buffer, f->numBlockGroups * sizeof(Ext2BlockGroupDescriptor));
    free(buffer);

    return true;
}

bool writeBGDT(struct Ext2File *f, uint32_t blockNum, Ext2BlockGroupDescriptor *bgdt) {
    uint8_t *buffer = malloc(f->blockSize);
    if (!buffer) return false;

    memset(buffer, 0, f->blockSize);
    memcpy(buffer, bgdt, f->numBlockGroups * sizeof(Ext2BlockGroupDescriptor));

    bool result = writeBlock(f, blockNum, buffer);
    free(buffer);

    return result;
}

void displaySuperblock(Ext2Superblock *sb) {
    printf("Superblock contents:\n");
    printf("Number of inodes: %u\n", sb->s_inodes_count);
    printf("Number of blocks: %u\n", sb->s_blocks_count);
    printf("Number of reserved blocks: %u\n", sb->s_r_blocks_count);
    printf("Number of free blocks: %u\n", sb->s_free_blocks_count);
    printf("Number of free inodes: %u\n", sb->s_free_inodes_count);
    printf("First data block: %u\n", sb->s_first_data_block);
    printf("Log block size: %u (%u)\n", sb->s_log_block_size, 1024 << sb->s_log_block_size);
    printf("Log fragment size: %u (%u)\n", sb->s_log_frag_size, 1024 << sb->s_log_frag_size);
    printf("Blocks per group: %u\n", sb->s_blocks_per_group);
    printf("Fragments per group: %u\n", sb->s_frags_per_group);
    printf("Inodes per group: %u\n", sb->s_inodes_per_group);
    printf("Last mount time: %s", ctime((time_t*)&sb->s_mtime));
    printf("Last write time: %s", ctime((time_t*)&sb->s_wtime));
    printf("Mount count: %u\n", sb->s_mnt_count);
    printf("Max mount count: %u\n", sb->s_max_mnt_count);
    printf("Magic number: 0x%x\n", sb->s_magic);
    printf("State: %u\n", sb->s_state);
    printf("Error processing: %u\n", sb->s_errors);
    printf("Revision level: %u.%u\n", sb->s_rev_level, sb->s_minor_rev_level);
    printf("Last system check: %s", ctime((time_t*)&sb->s_lastcheck));
    printf("Check interval: %u\n", sb->s_checkinterval);
    printf("OS creator: %u\n", sb->s_creator_os);
    printf("Default reserve UID: %u\n", sb->s_def_resuid);
    printf("Default reserve GID: %u\n", sb->s_def_resgid);
    printf("First inode number: %u\n", sb->s_first_ino);
    printf("Inode size: %u\n", sb->s_inode_size);
    printf("Block group number: %u\n", sb->s_block_group_nr);
    printf("Feature compatibility bits: 0x%08x\n", sb->s_feature_compat);
    printf("Feature incompatibility bits: 0x%08x\n", sb->s_feature_incompat);
    printf("Feature read/only compatibility bits: 0x%08x\n", sb->s_feature_ro_compat);
    printf("UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
        sb->s_uuid[0], sb->s_uuid[1], sb->s_uuid[2], sb->s_uuid[3],
        sb->s_uuid[4], sb->s_uuid[5], sb->s_uuid[6], sb->s_uuid[7],
        sb->s_uuid[8], sb->s_uuid[9], sb->s_uuid[10], sb->s_uuid[11],
        sb->s_uuid[12], sb->s_uuid[13], sb->s_uuid[14], sb->s_uuid[15]);
    printf("Volume name: [%s]\n", sb->s_volume_name);
    printf("Last mount point: [%s]\n", sb->s_last_mounted);
    printf("Algorithm bitmap: 0x%08x\n", sb->s_algorithm_usage_bitmap);
    printf("Number of blocks to preallocate: %u\n", sb->s_prealloc_blocks);
    printf("Number of blocks to preallocate for directories: %u\n", sb->s_prealloc_dir_blocks);
    printf("Journal UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
        sb->s_journal_uuid[0], sb->s_journal_uuid[1], sb->s_journal_uuid[2], sb->s_journal_uuid[3],
        sb->s_journal_uuid[4], sb->s_journal_uuid[5], sb->s_journal_uuid[6], sb->s_journal_uuid[7],
        sb->s_journal_uuid[8], sb->s_journal_uuid[9], sb->s_journal_uuid[10], sb->s_journal_uuid[11],
        sb->s_journal_uuid[12], sb->s_journal_uuid[13], sb->s_journal_uuid[14], sb->s_journal_uuid[15]);
    printf("Journal inode number: %u\n", sb->s_journal_inum);
    printf("Journal device number: %u\n", sb->s_journal_dev);
    printf("Journal last orphan inode number: %u\n", sb->s_last_orphan);
    printf("Default hash version: %u\n", sb->s_def_hash_version);
    printf("Default mount option bitmap: 0x%08x\n", sb->s_default_mount_opts);
    printf("First meta block group: %u\n", sb->s_first_meta_bg);
}

void displayBGDT(Ext2BlockGroupDescriptor *bgdt, uint32_t numBlockGroups) {
    printf("Block Group Descriptor Table:\n");
    printf("Block  Block  Inode  Inode  Free   Free   Used\n");
    printf("Number Bitmap Bitmap Table Blocks Inodes Dirs\n");
    printf("------------------------------------------------\n");

    for (uint32_t i = 0; i < numBlockGroups; i++) {
        printf("%5u %6u %6u %6u %6u %6u %4u\n",
            i,
            bgdt[i].bg_block_bitmap,
            bgdt[i].bg_inode_bitmap,
            bgdt[i].bg_inode_table,
            bgdt[i].bg_free_blocks_count,
            bgdt[i].bg_free_inodes_count,
            bgdt[i].bg_used_dirs_count);
    }
}