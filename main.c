#include "inode.h"
#include "ext2.h"
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

// Function prototypes
void displayBufferPage(uint8_t *buf, uint32_t count, uint32_t skip, uint64_t offset);
void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset);

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
