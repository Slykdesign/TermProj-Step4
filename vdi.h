#ifndef VDI_H
#define VDI_H

#include <stdio.h>
#include <stdint.h>

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

#endif