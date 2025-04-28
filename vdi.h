#ifndef VDI_H
#define VDI_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// VDI Header Structure (based on VirtualBox source and forum discussions)
typedef struct {
    char magic[4];          // "<<< "
    char creator[60];       // "Oracle VM VirtualBox Disk Image >>>\n"
    uint32_t signature;     // 0xbeda107f
    uint32_t version;       // 0x00010001
    uint32_t headerSize;    // 0x00000190 (400)
    uint32_t imageType;     // 0x00020000 Fixed; 0x00010000 Dynamic
    uint32_t flags;          // 0x00000000
    uint64_t virtualSize;    // Size of virtual disk in bytes
    uint32_t sectorSize;     // 512
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors;
    uint64_t diskOffset;
    uint32_t sectorsPerCylinder;
    uint32_t headsPerCylinder;
    uint64_t mapOffset;      // Offset to the translation table
    uint64_t frameOffset;    // Offset to the first data frame
    uint32_t frameSize;      // Size of each frame (page)
    uint32_t extraFrameSize; // unused
    uint32_t totalFrames;    // Number of frames
    uint32_t framesAllocated;// Number of frames allocated
    char uuid[16];
    char lastSnapUuid[16];
    char linkUuid[16];
    char parentUuid[16];
    char comment[256];       // Image comment

} VDIHeader;

typedef struct {
    int fd;
    uint8_t header[400];
    uint32_t *map;        // Translation map (logical-to-physical mapping)
    size_t cursor;        // Current read/write position
    uint32_t pageSize;    // Page size (from header)
    uint32_t totalPages;  // Number of allocated pages
} VDIFile;

VDIFile *vdiOpen(const char *filename);
void vdiClose(VDIFile *vdi);
ssize_t vdiRead(VDIFile *vdi, void *buf, size_t count);
ssize_t vdiWrite(VDIFile *vdi, void *buf, size_t count);
off_t vdiSeek(VDIFile *vdi, off_t offset, int anchor);
off_t vdiTranslate(VDIFile *vdi, off_t logicalOffset);
void displayVDIHeader(VDIFile *vdi);
void displayVDITranslationMap(VDIFile *vdi);
void displayMBR(VDIFile *vdi);
void displayBuffer(uint8_t *buf, uint32_t count, uint64_t offset);

#endif