#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define PAGE_FAULT -1

/**
 * Global variables just to avoid passing them / returning them
 * to / from the replacement policy functions.
 */
FILE *traceFile;
int numFrames = 0, numReads = 0, numWrites = 0;
char *replacementPolicy;
bool debug;

struct PageTableEntry {
    int pageNum;
    bool dirty;
    bool secondChance;
};

struct PageTable {
    struct PageTableEntry *entries;
    int numEntries;
    bool isFull;
};

// Helper functions
struct PageTable initPageTable();
unsigned int getPageNum(unsigned int address);
unsigned int findEntry(struct PageTable pageTable, unsigned int pageNum);
void printDebugInfo(struct PageTable pageTable);

// Replacement Policy functions.
void rdm();
void lru();
void fifo();
void vms();

int main(int argc, char *argv[])
{
    if (argc != 5) {
        printf("Usage: memsim <tracefile> <numframes> <rdm|lru|fifo|vms> "
        "<debug|quiet>\n");
        return -1;
    }

    traceFile = fopen(argv[1], "r");
    if (traceFile == NULL) {
        printf("Failed to open %s. Ensure proper file name and file is in " 
        "proper directory and try again.\n", argv[1]);
        return -1;
    }

    sscanf(argv[2], "%d", &numFrames);
    replacementPolicy = argv[3];
    char *debugStr = argv[4];
    if (strcmp(debugStr, "debug") == 0) {
        debug = true;
    }
    else {
        debug = false;
    }

    if (strcmp(replacementPolicy, "rdm") == 0) {
        rdm();
    }
    else if (strcmp(replacementPolicy, "lru") == 0) {
        lru();
    }
    else if (strcmp(replacementPolicy, "fifo") == 0) {
        fifo();
    }
    else if (strcmp(replacementPolicy, "vms") == 0) {
        vms();
    }
    else {
        printf("Unrecognized replacement policy. Options: rdm lru fifo vms\n");
    }



    return 0;
}

struct PageTable initPageTable()
{
    struct PageTable pageTable;
    pageTable.entries = malloc(numFrames * sizeof(struct PageTableEntry));

    for (int i = 0; i < numFrames; i++) {
        pageTable.entries[i].pageNum = -1;
        pageTable.entries[i].dirty = false;
    }

    pageTable.numEntries = 0;
    pageTable.isFull = false;

    return pageTable;
}

unsigned int getPageNum(unsigned int address)
{
    return (address & 0xFFFFF000) >> 12;
}

unsigned int findEntry(struct PageTable pageTable, unsigned int pageNum)
{
    for (int i = 0; i < numFrames; i++) {
        if (pageTable.entries[i].pageNum == pageNum) {
            return i;
        }
    }

    return PAGE_FAULT;
}

void printDebugInfo(struct PageTable pageTable)
{
    printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);

    printf("PAGE TABLE\n");
    printf("numEntries: %-6d isFull: %d\n", pageTable.numEntries, pageTable.isFull);
    printf("============================\n");
    printf("Entry: PageNumber: Dirty:\n");
    for (int i = 0; i < numFrames; i++) {
        printf("%-6d 0x%08x  %d\n", i, pageTable.entries[i].pageNum, pageTable.entries[i].dirty);
    }
}

void rdm()
{
    srand(time(0));

    struct PageTable pageTable = initPageTable();

    unsigned int address, pageNum;
    char rw, exitCh;

    int counter = 1, index = 0, randIndex = 0;
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        pageNum = getPageNum(address);

        if (debug) {
            printf("Address: 0x%08x RW: %c PageNum: 0x%08x\n", address, rw, pageNum);
            counter++;
            if (counter % 11 == 0) {
                printDebugInfo(pageTable);
                printf("Enter x to exit. ");

                exitCh = getchar();
                if (exitCh == 'X' || exitCh == 'x') {
                    free(pageTable.entries);
                    exit(0);
                }
            }
        }
        
        index = findEntry(pageTable, pageNum);

        if (index != PAGE_FAULT) {
            if (rw == 'R') {
                continue;
            }
            else {
                pageTable.entries[index].dirty = true;
            }
        }
        else {
            if (!pageTable.isFull && rw == 'R') {
                pageTable.entries[pageTable.numEntries].pageNum = pageNum;
                pageTable.numEntries++;
                numReads++;

                if (pageTable.numEntries == numFrames) {
                    pageTable.isFull = true;
                }
            }
            else if (!pageTable.isFull && rw == 'W') {

            }
            else if (pageTable.isFull && rw == 'R') {
                randIndex = rand() % numFrames;
                
                
                if (pageTable.entries[randIndex].dirty) {
                    numWrites++;
                }

                pageTable.entries[randIndex].pageNum = pageNum;
                pageTable.entries[randIndex].dirty = false;
                numReads++;
            }
            else if (pageTable.isFull && rw == 'W') {
                numWrites++;
            }
            
        }

    }

    free(pageTable.entries);
}

void lru()
{

}

void fifo()
{

}

void vms()
{

}
