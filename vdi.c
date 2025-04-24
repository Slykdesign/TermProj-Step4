#include "vdi.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

VDIFile *vdiOpen(const char *filename) {
    VDIFile *vdi = malloc(sizeof(VDIFile));
    if (!vdi) return NULL;

    vdi->fd = open(filename, O_RDWR);
    if (vdi->fd == -1) {
        perror("Error opening VDI file");
        free(vdi);
        return NULL;
    }

    // Read VDI header
    if (read(vdi->fd, vdi->header, 400) != 400) {
        perror("Error reading VDI header");
        close(vdi->fd);
        free(vdi);
        return NULL;
    }

    // Extract relevant data
    vdi->pageSize = *(uint32_t *)(vdi->header + 36); // Example offset for page size
    vdi->totalPages = *(uint32_t *)(vdi->header + 40); // Example offset for total pages

    // Read translation map
    off_t mapOffset = *(off_t *)(vdi->header + 16); // Example offset for map
    lseek(vdi->fd, mapOffset, SEEK_SET);
    vdi->map = malloc(vdi->totalPages * sizeof(uint32_t));
    if (!vdi->map) {
        perror("Error allocating memory for translation map");
        close(vdi->fd);
        free(vdi);
        return NULL;
    }
    read(vdi->fd, vdi->map, vdi->totalPages * sizeof(uint32_t));

    vdi->cursor = 0;
    return vdi;
}

void vdiClose(VDIFile *vdi) {
    if (vdi) {
        close(vdi->fd);
        free(vdi->map);
        free(vdi);
    }
}

ssize_t vdiRead(VDIFile *vdi, void *buf, size_t count) {
    size_t bytesRead = 0;
    uint8_t *buffer = (uint8_t *)buf;

    while (count > 0) {
        off_t physicalOffset = vdiTranslate(vdi, vdi->cursor);
        if (physicalOffset == -1) {
            return -1; // Return error if translation fails
        }

        size_t pageRemaining = vdi->pageSize - (vdi->cursor % vdi->pageSize);
        size_t toRead = (count < pageRemaining) ? count : pageRemaining;

        lseek(vdi->fd, physicalOffset, SEEK_SET);
        ssize_t result = read(vdi->fd, buffer, toRead);
        if (result <= 0) break;

        bytesRead += result;
        buffer += result;
        count -= result;
        vdi->cursor += result;
    }
    return bytesRead;
}

ssize_t vdiWrite(VDIFile *vdi, void *buf, size_t count) {
    size_t bytesWritten = 0;
    uint8_t *buffer = (uint8_t *)buf;

    while (count > 0) {
        off_t physicalOffset = vdiTranslate(vdi, vdi->cursor);
        if (physicalOffset == -1) {
            return bytesWritten;  // Writing to unallocated pages is not supported yet
        }

        size_t pageRemaining = vdi->pageSize - (vdi->cursor % vdi->pageSize);
        size_t toWrite = (count < pageRemaining) ? count : pageRemaining;

        lseek(vdi->fd, physicalOffset, SEEK_SET);
        ssize_t result = write(vdi->fd, buffer, toWrite);
        if (result <= 0) break;

        bytesWritten += result;
        buffer += result;
        count -= result;
        vdi->cursor += result;
    }
    return bytesWritten;
}

off_t vdiSeek(VDIFile *vdi, off_t offset, int anchor) {
    if (!vdi) return -1;

    off_t newCursor;
    if (anchor == SEEK_SET) {
        newCursor = offset;
    } else if (anchor == SEEK_CUR) {
        newCursor = vdi->cursor + offset;
    } else if (anchor == SEEK_END) {
        newCursor = 134217728 + offset;  // Adjust disk size accordingly
    } else {
        return -1;
    }

    if (newCursor < 0 || newCursor > 134217728) return -1;

    vdi->cursor = newCursor;
    return vdi->cursor;
}

off_t vdiTranslate(VDIFile *vdi, off_t logicalOffset) {
    uint32_t pageNum = logicalOffset / vdi->pageSize;
    uint32_t offsetInPage = logicalOffset % vdi->pageSize;

    if (pageNum >= vdi->totalPages) {
        return -1;  // Out of range
    }

    uint32_t physicalPage = vdi->map[pageNum];

    if (physicalPage == 0xFFFFFFFF) {
        return -1; // Page not allocated, return error or zeroed buffer
    }

    return (off_t)(physicalPage * vdi->pageSize + offsetInPage);
}

void displayVDIHeader(VDIFile *vdi) {
    printf("      Image name: [<<< Oracle VM VirtualBox Disk Image >>>        ]\n");
    printf("       Signature: 0x%x\n", *(uint32_t *)(vdi->header));
    printf("         Version: %d.%d\n", vdi->header[4], vdi->header[5]);
    printf("     Header size: 0x%08x  %d\n", *(uint32_t *)(vdi->header + 12), *(uint32_t *)(vdi->header + 12));
    printf("      Image type: 0x%08x\n", *(uint32_t *)(vdi->header + 16));
    printf("           Flags: 0x%08x\n", *(uint32_t *)(vdi->header + 20));
    printf("     Virtual CHS: %d-%d-%d\n", *(uint16_t *)(vdi->header + 24), *(uint16_t *)(vdi->header + 26), *(uint16_t *)(vdi->header + 28));
    printf("     Sector size: 0x%08x  %d\n", *(uint32_t *)(vdi->header + 32), *(uint32_t *)(vdi->header + 32));
    printf("     Logical CHS: %d-%d-%d\n", *(uint16_t *)(vdi->header + 36), *(uint16_t *)(vdi->header + 38), *(uint16_t *)(vdi->header + 40));
    printf("      Map offset: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 44), *(uint32_t *)(vdi->header + 44));
    printf("    Frame offset: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 48), *(uint32_t *)(vdi->header + 48));
    printf("      Frame size: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 52), *(uint32_t *)(vdi->header + 52));
    printf("Extra frame size: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 56), *(uint32_t *)(vdi->header + 56));
    printf("    Total frames: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 60), *(uint32_t *)(vdi->header + 60));
    printf("Frames allocated: 0x%08lx  %ld\n", *(uint32_t *)(vdi->header + 64), *(uint32_t *)(vdi->header + 64));
    printf("       Disk size: 0x%016llx  %llu\n", *(uint64_t *)(vdi->header + 68), *(uint64_t *)(vdi->header + 68));
    printf("            UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
           vdi->header[80], vdi->header[81], vdi->header[82], vdi->header[83], vdi->header[84], vdi->header[85],
           vdi->header[86], vdi->header[87], vdi->header[88], vdi->header[89], vdi->header[90], vdi->header[91],
           vdi->header[92], vdi->header[93], vdi->header[94], vdi->header[95]);
    printf("  Last snap UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
           vdi->header[96], vdi->header[97], vdi->header[98], vdi->header[99], vdi->header[100], vdi->header[101],
           vdi->header[102], vdi->header[103], vdi->header[104], vdi->header[105], vdi->header[106], vdi->header[107],
           vdi->header[108], vdi->header[109], vdi->header[110], vdi->header[111]);
    printf("       Link UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
           vdi->header[112], vdi->header[113], vdi->header[114], vdi->header[115], vdi->header[116], vdi->header[117],
           vdi->header[118], vdi->header[119], vdi->header[120], vdi->header[121], vdi->header[122], vdi->header[123],
           vdi->header[124], vdi->header[125], vdi->header[126], vdi->header[127]);
    printf("     Parent UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
           vdi->header[128], vdi->header[129], vdi->header[130], vdi->header[131], vdi->header[132], vdi->header[133],
           vdi->header[134], vdi->header[135], vdi->header[136], vdi->header[137], vdi->header[138], vdi->header[139],
           vdi->header[140], vdi->header[141], vdi->header[142], vdi->header[143]);
    printf("   Image comment:\n");

    // Display the rest of the header for debugging
    displayBuffer(vdi->header, 400, 0);
}

void displayVDITranslationMap(VDIFile *vdi) {
    uint8_t mbr[512];
    vdiSeek(vdi, 0, SEEK_SET);
    vdiRead(vdi, mbr, 512);
    displayBuffer(mbr, 512, 0x0);
}

void displayMBR(VDIFile *vdi) {
    uint8_t mbr[256];
    vdiSeek(vdi, 0, SEEK_SET);
    vdiRead(vdi, mbr, sizeof(mbr));
    displayBuffer(mbr, sizeof(mbr), 0);
}