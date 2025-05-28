#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 64
#define SUPERBLOCK_BLOCK_NO 0
#define INODE_BITMAP_BLOCK_NO 1
#define DATA_BITMAP_BLOCK_NO 2
#define INODE_TABLE_START_BLOCK 3
#define INODE_TABLE_BLOCKS 5
#define DATA_BLOCK_START 8
#define INODE_SIZE 256
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define MAX_INODES (INODES_PER_BLOCK * INODE_TABLE_BLOCKS)
#define MAX_DATA_BLOCKS (TOTAL_BLOCKS - DATA_BLOCK_START)

/**
 * Superblock structure representing the filesystem metadata.
 */
typedef struct {
    uint16_t magic;
    uint32_t blockSize;
    uint32_t totalBlocks;
    uint32_t inodeBitmapBlock;
    uint32_t dataBitmapBlock;
    uint32_t inodeTableStart;
    uint32_t dataBlockStart;
    uint32_t inodeSize;
    uint32_t inodeCount;
    char reserved[4058];
} __attribute__((packed)) Superblock;

/**
 * Inode structure representing a file or directory metadata.
 */
typedef struct {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint32_t links;
    uint32_t blocks;
    uint32_t direct;
    uint32_t indirect;
    uint32_t doubleIndirect;
    uint32_t tripleIndirect;
    char reserved[156];
} __attribute__((packed)) Inode;

// Global arrays to track block and inode usage
bool dataBitmap[MAX_DATA_BLOCKS];
bool dataBlockUsed[MAX_DATA_BLOCKS];
int blockRefCount[MAX_DATA_BLOCKS];
bool inodeBitmap[MAX_INODES];
bool inodeUsed[MAX_INODES];

//reads a block from the filesystem image into the provided buffer.
void readBlock(FILE *fp, int blockNum, void *buffer) {
    fseek(fp, blockNum * BLOCK_SIZE, SEEK_SET);
    fread(buffer, BLOCK_SIZE, 1, fp);
}

//Loads a bitmap from a specified block into a boolean array.
void loadBitmap(FILE *fp, int blockNum, bool *bitmap, int count) {
    uint8_t rawBitmap[BLOCK_SIZE];
    readBlock(fp, blockNum, rawBitmap);
    for (int i = 0; i < count; i++) {
        bitmap[i] = (rawBitmap[i / 8] >> (i % 8)) & 1;
    }
}
//Feature 1: Superblock Validates the superblock fields and prints errors if any are invalid.
void readSuperblock(FILE *fp, Superblock *superblock) {
    readBlock(fp, SUPERBLOCK_BLOCK_NO, superblock);

    if (superblock->magic != 0xD34D) {
        printf("ERROR: Invalid magic number in superblock.\n");
    }
    if (superblock->blockSize != BLOCK_SIZE) {
        printf("ERROR: Invalid block size in superblock.\n");
    }
    if (superblock->totalBlocks != TOTAL_BLOCKS) {
        printf("ERROR: Invalid total block count in superblock.\n");
    }

    if (superblock->inodeBitmapBlock != INODE_BITMAP_BLOCK_NO ||
        superblock->dataBitmapBlock != DATA_BITMAP_BLOCK_NO ||
        superblock->inodeTableStart != INODE_TABLE_START_BLOCK ||
        superblock->dataBlockStart != DATA_BLOCK_START) {
        printf("ERROR: One or more superblock pointers are incorrect.\n");
    }

    if (superblock->inodeSize != INODE_SIZE) {
        printf("ERROR: Invalid inode size in superblock.\n");
    }
    if (superblock->inodeCount > MAX_INODES) {
        printf("ERROR: Inode count in superblock exceeds maximum allowed.\n");
    }
}

/**
 * Checks all inodes for validity and consistency with inode bitmap.
 * Updates global arrays tracking used inodes and data blocks.
 */
void checkInodes(FILE *fp) {
    Inode inode;
    uint8_t blockBuffer[BLOCK_SIZE];

    for (int i = 0; i < MAX_INODES; i++) {
        int blockNum = INODE_TABLE_START_BLOCK + (i * INODE_SIZE) / BLOCK_SIZE;
        int offset = (i * INODE_SIZE) % BLOCK_SIZE;

        readBlock(fp, blockNum, blockBuffer);
        memcpy(&inode, blockBuffer + offset, sizeof(Inode));

        bool isValid = (inode.links > 0 && inode.dtime == 0);

        if (inodeBitmap[i] && !isValid) {
            printf("ERROR: Inode %d marked used in bitmap but is invalid.\n", i);
        }

        if (!inodeBitmap[i] && isValid) {
            printf("ERROR: Inode %d is valid but not marked used in bitmap.\n", i);
        }

        if (isValid) {
            inodeUsed[i] = true;

            // Check direct block reference
            if (inode.direct >= MAX_DATA_BLOCKS) {
                printf("ERROR: Inode %d has invalid direct block %u.\n", i, inode.direct);
            } else {
                dataBlockUsed[inode.direct] = true;
                blockRefCount[inode.direct]++;
                if (!dataBitmap[inode.direct]) {
                    printf("ERROR: Inode %d references block %u not marked in data bitmap.\n", i, inode.direct);
                }
            }
        }
    }
}

/**
 * Checks the consistency of the data bitmap against actual data block usage.
 */
/**
 * Feature 2: Data Bitmap Consistency Checker
 * Checks the consistency of the data bitmap against actual data block usage.
 */
void checkDataBitmap() {
    for (int i = 0; i < MAX_DATA_BLOCKS; i++) {
        if (dataBitmap[i] && !dataBlockUsed[i]) {
            printf("ERROR: Data block %d marked used in bitmap but not referenced.\n", i);
        }

        if (!dataBitmap[i] && dataBlockUsed[i]) {
            printf("ERROR: Data block %d is used but not marked in bitmap.\n", i);
        }

        if (blockRefCount[i] > 1) {
            printf("ERROR: Data block %d is referenced by multiple inodes.\n", i);
        }
    }
}

/**
 * Checks the consistency of the inode bitmap against actual inode usage.
 */
/**
 * Feature 3: Inode Bitmap Consistency Checker
 * Checks the consistency of the inode bitmap against actual inode usage.
 */
void checkInodeBitmap() {
    int errorCount = 0;
    for (int i = 0; i < MAX_INODES; i++) {
        if (inodeBitmap[i] && !inodeUsed[i]) {
            errorCount++;
            printf("ERROR: Inode %d marked used but not actually used.\n", i);
        }

        if (!inodeBitmap[i] && inodeUsed[i]) {
            errorCount++;
            printf("ERROR: Inode %d is used but not marked in bitmap.\n", i);
        }
    }
    if (errorCount == 0) {
        printf("Inode bitmap is consistent.\n");
    }
}

/**
 * Checks for duplicate data block references by multiple inodes.
 */
/**
 * Feature 4: Duplicate Block Checker
 * Checks for duplicate data block references by multiple inodes.
 */
void checkDuplicateBlocks() {
    bool duplicateFound = false;
    for (int i = 0; i < MAX_DATA_BLOCKS; i++) {
        if (blockRefCount[i] > 1) {
            printf("ERROR: Data block %d is referenced by multiple inodes.\n", i);
            duplicateFound = true;
        }
    }
    if (!duplicateFound) {
        printf("No duplicate data block references found.\n");
    }
}

/**
 * Checks for invalid block references in direct and indirect pointers of inodes.
 */
/**
 * Feature 5: Bad Block Checker
 * Checks for invalid block references in direct and indirect pointers of inodes.
 */
void checkBadBlocks(FILE *fp) {
    Inode inode;
    uint8_t blockBuffer[BLOCK_SIZE];
    bool badBlockFound = false;

    for (int i = 0; i < MAX_INODES; i++) {
        int blockNum = INODE_TABLE_START_BLOCK + (i * INODE_SIZE) / BLOCK_SIZE;
        int offset = (i * INODE_SIZE) % BLOCK_SIZE;

        readBlock(fp, blockNum, blockBuffer);
        memcpy(&inode, blockBuffer + offset, sizeof(Inode));

        bool isValid = (inode.links > 0 && inode.dtime == 0);
        if (!isValid) {
            continue;
        }

        if (inode.direct >= MAX_DATA_BLOCKS) {
            printf("ERROR: Inode %d has invalid direct block %u.\n", i, inode.direct);
            badBlockFound = true;
        }

        if (inode.indirect >= MAX_DATA_BLOCKS && inode.indirect != 0) {
            printf("ERROR: Inode %d has invalid single indirect block %u.\n", i, inode.indirect);
            badBlockFound = true;
        }

        if (inode.doubleIndirect >= MAX_DATA_BLOCKS && inode.doubleIndirect != 0) {
            printf("ERROR: Inode %d has invalid double indirect block %u.\n", i, inode.doubleIndirect);
            badBlockFound = true;
        }

        if (inode.tripleIndirect >= MAX_DATA_BLOCKS && inode.tripleIndirect != 0) {
            printf("ERROR: Inode %d has invalid triple indirect block %u.\n", i, inode.tripleIndirect);
            badBlockFound = true;
        }
    }
    if (!badBlockFound) {
        printf("No invalid block references found in inodes.\n");
    }
}

/**
 * Main entry point of the program.
 * Validates command line arguments, opens the image file, and runs all checks.
 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <vsfs.img>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Failed to open image file");
        return EXIT_FAILURE;
    }

    Superblock superblock;
    readSuperblock(fp, &superblock);
    printf("Superblock validation completed.\n");

    loadBitmap(fp, INODE_BITMAP_BLOCK_NO, inodeBitmap, MAX_INODES);
    loadBitmap(fp, DATA_BITMAP_BLOCK_NO, dataBitmap, MAX_DATA_BLOCKS);
    printf("Bitmaps loaded successfully.\n");

    checkInodes(fp);
    printf("Inode checks completed.\n");

    checkInodeBitmap();
    checkDataBitmap();
    printf("Bitmap consistency checks completed.\n");

    checkDuplicateBlocks();
    checkBadBlocks(fp);
    printf("Block reference checks completed.\n");

    fclose(fp);
    return EXIT_SUCCESS;
}
