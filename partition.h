#ifndef PARTITION_H
#define PARTITION_H

#include "vdi.h"

typedef struct {
    uint8_t status;          // Bootable flag
    uint8_t firstCHS[3];     // CHS address of the first sector
    uint8_t partitionType;   // Partition type
    uint8_t lastCHS[3];      // CHS address of the last sector
    uint32_t firstLBA;       // LBA of the first sector
    uint32_t sectorCount;    // Number of sectors in the partition
} MBRPartitionEntry;

typedef struct {
    VDIFile *vdi;
    uint8_t partitionTable[64];  // 4 entries * 16 bytes each
    uint32_t startSector;
    uint32_t sectorCount;
    size_t cursor;
} MBRPartition;

MBRPartition* openPartition(const char *filename, int part);
void closePartition(MBRPartition *partition);
ssize_t vdiReadPartition(MBRPartition *partition, void *buf, size_t count);
ssize_t writePartition(MBRPartition *partition, void *buf, size_t count);
off_t vdiSeekPartition(MBRPartition *partition, off_t offset, int anchor);
void displayPartitionTable(MBRPartition *partition);

#endif