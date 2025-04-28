#include "partition.h"
#include "vdi.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

MBRPartition* openPartition(const char *filename, int part) {
    MBRPartition *partition = malloc(sizeof(MBRPartition));
    if (!partition) return NULL;

    partition->vdi = vdiOpen(filename);
    if (!partition->vdi) {
        free(partition);
        return NULL;
    }

    lseek(partition->vdi->fd, 446, SEEK_SET);  // MBR partition table starts at byte 446
    read(partition->vdi->fd, partition->partitionTable, 64);

    uint8_t *entry = partition->partitionTable + part * 16;
    partition->startSector = *(uint32_t *)(entry + 8);
    partition->sectorCount = *(uint32_t *)(entry + 12);
    partition->cursor = 0;

    return partition;
}

void closePartition(MBRPartition *partition) {
    if (partition) {
        vdiClose(partition->vdi);
        free(partition);
    }
}

ssize_t vdiReadPartition(MBRPartition *partition, void *buf, size_t count) {
    size_t bytesRead = 0;
    uint8_t *buffer = (uint8_t *)buf;

    while (count > 0) {
        off_t logicalOffset = partition->startSector * 512 + partition->cursor;
        off_t physicalOffset = vdiTranslate(partition->vdi, logicalOffset);
        if (physicalOffset == -1) return bytesRead;

        size_t toRead = (count < 512) ? count : 512;
        lseek(partition->vdi->fd, physicalOffset, SEEK_SET);
        ssize_t result = read(partition->vdi->fd, buffer, toRead);
        if (result <= 0) break;

        bytesRead += result;
        buffer += result;
        count -= result;
        partition->cursor += result;
    }
    return bytesRead;
}

ssize_t writePartition(MBRPartition *partition, void *buf, size_t count) {
    size_t bytesWritten = 0;
    uint8_t *buffer = (uint8_t *)buf;

    while (count > 0) {
        off_t logicalOffset = partition->startSector * 512 + partition->cursor;
        off_t physicalOffset = vdiTranslate(partition->vdi, logicalOffset);
        if (physicalOffset == -1) return bytesWritten;

        size_t toWrite = (count < 512) ? count : 512;
        lseek(partition->vdi->fd, physicalOffset, SEEK_SET);
        ssize_t result = write(partition->vdi->fd, buffer, toWrite);
        if (result <= 0) break;

        bytesWritten += result;
        buffer += result;
        count -= result;
        partition->cursor += result;
    }
    return bytesWritten;
}

off_t vdiSeekPartition(MBRPartition *partition, off_t offset, int anchor) {
    off_t newCursor = 0;  // <-- initialize

    if (anchor == SEEK_SET) {
        newCursor = offset;
    } else if (anchor == SEEK_CUR) {
        newCursor = partition->cursor + offset;
    } else if (anchor == SEEK_END) {
        newCursor = partition->sectorCount * 512 + offset;
    } else {
        return -1;
    }

    if (newCursor < 0 || newCursor > (off_t)(partition->sectorCount * 512))
        return -1;

    partition->cursor = newCursor;
    return partition->cursor;
}

void displayPartitionTable(MBRPartition *partition) {
    for (int i = 0; i < 4; i++) {
        uint8_t *entry = partition->partitionTable + i * 16;
        printf("Partition table entry %d:\n", i);
        printf("  Status: %s\n", (entry[0] == 0x80) ? "Active" : "Inactive");
        printf("  First sector CHS: %d-%d-%d\n", entry[1], entry[2], entry[3]);
        printf("  Last sector CHS: %d-%d-%d\n", entry[5], entry[6], entry[7]);
        printf("  Partition type: %02x\n", entry[4]);
        printf("  First LBA sector: %u\n", *(uint32_t *)(entry + 8));
        printf("  LBA sector count: %u\n", *(uint32_t *)(entry + 12));
    }
}