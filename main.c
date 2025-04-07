#include "partition.h"
#include "ext2.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <ctype.h>

// Function prototypes
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset);
void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset);
void displaySuperblock(Ext2Superblock *sb);
void displayBGDT(Ext2BlockGroupDescriptor *bgdt, uint32_t num_block_groups);
void displayInode(Inode *inode);

int main() {
    Ext2File *ext2 = openExt2File("./good-fixed-1k.vdi");
    if (!ext2) return 1;

    printf("Superblock from block 0\n");
    displaySuperblock(&ext2->superblock);

    printf("\nBlock group descriptor table:\n");
    displayBGDT(ext2->bgdt, ext2->num_block_groups);

    printf("\nFetching inode 1:\n");
    Inode inode;
    if (fetchInode(ext2, 1, &inode) == 0) {
        displayInode(&inode);
    } else {
        printf("Failed to fetch inode 1\n");
    }

    printf("\nAllocating new inode:\n");
    uint32_t new_inode_num = allocateInode(ext2, -1);
    if (new_inode_num) {
        printf("Allocated inode number: %u\n", new_inode_num);
        if (fetchInode(ext2, new_inode_num, &inode) == 0) {
            displayInode(&inode);
        } else {
            printf("Failed to fetch allocated inode %u\n", new_inode_num);
        }
    } else {
        printf("Failed to allocate new inode\n");
    }

    closeExt2File(ext2);
    return 0;
}

#include "partition.h"
#include "ext2.h"
#include "file_access.h" // Add this include
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <ctype.h>

// Function prototypes
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset);
void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset);
void displaySuperblock(Ext2Superblock *sb);
void displayBGDT(Ext2BlockGroupDescriptor *bgdt, uint32_t num_block_groups);
void displayInode(Inode *inode);

int main() {
    Ext2File *ext2 = openExt2File("./good-fixed-1k.vdi");
    if (!ext2) return 1;

    printf("Superblock from block 0\n");
    displaySuperblock(&ext2->superblock);

    printf("\nBlock group descriptor table:\n");
    displayBGDT(ext2->bgdt, ext2->num_block_groups);

    printf("\nFetching inode 1:\n");
    Inode inode;
    if (fetchInode(ext2, 1, &inode) == 0) {
        displayInode(&inode);
    } else {
        printf("Failed to fetch inode 1\n");
    }

    printf("\nAllocating new inode:\n");
    uint32_t new_inode_num = allocateInode(ext2, -1);
    if (new_inode_num) {
        printf("Allocated inode number: %u\n", new_inode_num);
        if (fetchInode(ext2, new_inode_num, &inode) == 0) {
            displayInode(&inode);
        } else {
            printf("Failed to fetch allocated inode %u\n", new_inode_num);
        }
    } else {
        printf("Failed to allocate new inode\n");
    }

    // New code to demonstrate fetching and writing blocks
    printf("\nFetching block 0 of the new inode:\n");
    uint8_t buffer[1024];
    if (fetchBlockFromFile(&inode, 0, buffer) == 0) {
        displayBuffer(buffer, 1024, 0);
    } else {
        printf("Failed to fetch block 0 of the new inode\n");
    }

    printf("\nWriting to block 0 of the new inode:\n");
    for (int i = 0; i < 1024; i++) {
        buffer[i] = i % 256; // Fill the buffer with some data
    }
    if (writeBlockToFile(&inode, 0, buffer) == 0) {
        printf("Successfully wrote to block 0 of the new inode\n");
    } else {
        printf("Failed to write to block 0 of the new inode\n");
    }

    closeExt2File(ext2);
    return 0;
}

// Function Definitions
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset) {
    printf("Offset: 0x%lx\n", offset);
    for (uint32_t i = skip; i < count; i += 16) {
        printf("%04x | ", i);
        for (uint32_t j = 0; j < 16 && i + j < count; j++) {
            printf("%02x ", buf[i + j]);
        }
        printf("| ");
        for (uint32_t j = 0; j < 16 && i + j < count; j++) {
            printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
        }
        printf("\n");
    }
}

void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset) {
    uint32_t step = 256;
    for (uint32_t i = 0; i < count; i += step) {
        displayBufferPage(buf, count, i, offset + i);
    }
}

// Function Definitions
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset) {
    printf("Offset: 0x%lx\n", offset);
    for (uint32_t i = skip; i < count; i += 16) {
        printf("%04x | ", i);
        for (uint32_t j = 0; j < 16 && i + j < count; j++) {
            printf("%02x ", buf[i + j]);
        }
        printf("| ");
        for (uint32_t j = 0; j < 16 && i + j < count; j++) {
            printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
        }
        printf("\n");
    }
}

void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset) {
    uint32_t step = 256;
    for (uint32_t i = 0; i < count; i += step) {
        displayBufferPage(buf, count, i, offset + i);
    }
}
