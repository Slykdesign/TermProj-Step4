#ifndef EXT2_H
#define EXT2_H

#include "partition.h"
#include <stdint.h>

#define EXT2_SUPERBLOCK_OFFSET 1024  // Offset of the superblock in bytes
#define EXT2_SUPERBLOCK_SIZE sizeof(Ext2Superblock)  // Size of the superblock structure
#define EXT2_SUPER_MAGIC 0xEF53

typedef struct {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
} Ext2Superblock;

typedef struct {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} Ext2BlockGroupDescriptor;

struct Ext2File {
    FILE *fd;
    VDIFile *vdi;
    MBRPartition *partition;
    uint64_t partitionOffset; // Start of ext2 partition in bytes
    Ext2Superblock superblock;
    Ext2BlockGroupDescriptor *bgdt;
    uint32_t blockSize;
    uint32_t numBlockGroups;
    uint8_t *cache; // Optional: Block cache
    uint32_t firstSector;
};

struct Ext2File *openExt2(char *fn);
void closeExt2(struct Ext2File *f);
bool fetchBlock(struct Ext2File *f, uint32_t blockNum, void *buf);
bool writeBlock(struct Ext2File *f, uint32_t blockNum, void *buf);
bool fetchSuperblock(struct Ext2File *f, uint32_t blockNum, Ext2Superblock *sb);
bool writeSuperblock(struct Ext2File *f, uint32_t blockNum, Ext2Superblock *sb);
bool fetchBGDT(struct Ext2File *f, uint32_t blockNum, Ext2BlockGroupDescriptor *bgdt);
bool writeBGDT(struct Ext2File *f, uint32_t blockNum, Ext2BlockGroupDescriptor *bgdt);
void displaySuperblock(Ext2Superblock *sb);
void displayBGDT(Ext2BlockGroupDescriptor *bgdt, uint32_t numBlockGroups);

#endif