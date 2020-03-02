#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define PROCESS_B 0x30000000

struct PageTableEntry
{
    int pageNum;
    bool dirty;
};

struct PageTable
{
    struct PageTableEntry *entries;
    int numEntries;
    bool isFull;
};

struct Node
{
    struct PageTableEntry *page;
    struct Node *prev;
    struct Node *next;
};

/**
 * Linked list used to keep track of FIFO / recency.
 * Nodes at the front of the list were added / accessed
 * most recently, while those in the back are candidates
 * for eviction.
 */
struct DLinkedList
{
    int numNodes;
    struct Node *header;
    struct Node *trailer;
};

// Page table functions.
struct PageTable initPageTable();
void printPageTable(struct PageTable pageTable);
struct PageTableEntry *findEntry(struct PageTable pageTable, unsigned int pageNum);

// Linked list functions.
struct DLinkedList initLinkedList();
void addFront(struct DLinkedList *list, struct PageTableEntry *page);
void rmFront(struct DLinkedList *list);
void insertFront(struct DLinkedList *list, struct Node *node);
void addBack(struct DLinkedList *list, struct PageTableEntry *page);
void rmBack(struct DLinkedList *list);
void insertBack(struct DLinkedList *list, struct Node *node);
struct Node *findNode(struct DLinkedList *list, struct PageTableEntry *page);
void rmNode(struct DLinkedList *list, struct PageTableEntry *page);
void updateRecency(struct DLinkedList *list, struct PageTableEntry *page);
struct PageTableEntry *getLeastRecent(struct DLinkedList *list);
void freeList(struct DLinkedList *list);
void printList(struct DLinkedList *list);

// Helper functions.
unsigned int getPageNum(unsigned int address);
unsigned int getProcess(unsigned int address);

// Replacement policy functions.
void rdm();
void lru();
void fifo();
void vms();

// Global variables.
FILE *traceFile;
int numFrames = 0, numReads = 0, 
numWrites = 0, numEvents = 0;
char *replacementPolicy;
bool debug;

int main(int argc, char *argv[])
{
    // Check for proper number of arguments.
    if (argc != 5) {
        printf("Usage: memsim <tracefile> <numframes> <rdm|lru|fifo|vms> "
        "<debug|quiet>\n");
        return -1;
    }

    // Open trace file, error check.
    traceFile = fopen(argv[1], "r");
    if (traceFile == NULL) {
        printf("Failed to open %s. Ensure proper file name and file is in " 
        "proper directory and try again.\n", argv[1]);
        return -1;
    }

    // Read in rest of arguments.
    sscanf(argv[2], "%d", &numFrames);
    replacementPolicy = argv[3];
    char *debugStr = argv[4];
    debug = strcmp(debugStr, "debug") == 0 ? true : false;

    // Execute passed replacement policy.
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
        return -1;
    }

    // Final output.
    printf("Total memory frames: %d\n", numFrames);
    printf("Events in trace: %d\n", numEvents);
    printf("Total disk reads: %d\n", numReads);
    printf("Total disk writes: %d\n", numWrites);

    return 0;
}

// Initialize empty page table of size numFrames.
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

// Print page table for debugging.
void printPageTable(struct PageTable pageTable)
{
    printf("PAGE TABLE\n");
    printf("numEntries: %-6d isFull: %d\n", pageTable.numEntries, pageTable.isFull);
    printf("============================\n");
    printf("Entry: PageNumber:    Dirty:\n");
    for (int i = 0; i < numFrames; i++) {
        printf("%-6d 0x%08x     %d\n", i, pageTable.entries[i].pageNum, pageTable.entries[i].dirty);
    }
    printf("============================\n");
}

// Find the PTE with the passed pageNum. If not found, returns NULL.
struct PageTableEntry *findEntry(struct PageTable pageTable, unsigned int pageNum)
{
    for (int i = 0; i < numFrames; i++) {
        if (pageTable.entries[i].pageNum == pageNum) {
            return &pageTable.entries[i];
        }
    }

    return NULL;
}

// Initialize an empty linked list with sentinel nodes.
struct DLinkedList initLinkedList()
{
    
    struct DLinkedList list;
    
    list.header = malloc(sizeof(struct Node));
    list.trailer = malloc(sizeof(struct Node));

    list.numNodes = 0;

    list.header->page = NULL;
    list.header->next = list.trailer;
    list.header->prev = NULL;

    list.trailer->page = NULL;
    list.trailer->prev = list.header;
    list.trailer->next = NULL;

    return list;
}

// Add the passed PTE to the front of the passed linked list.
void addFront(struct DLinkedList *list, struct PageTableEntry *page)
{
    
    struct Node *newNode = malloc(sizeof(struct Node));

    newNode->page = page;

    newNode->prev = list->header;
    newNode->next = list->header->next;
    list->header->next->prev = newNode;
    list->header->next = newNode;

    list->numNodes++;
}

// Remove the PTE at the front of the passed linked list.
void rmFront(struct DLinkedList *list)
{
    struct Node *node = list->header->next;

    if (node != list->trailer) {
        
        node->prev->next = node->next;
        node->next->prev = node->prev;

        free(node);

        list->numNodes--;
    }
}

// Insert the passed node within the passed linked list to the front of the list.
void insertFront(struct DLinkedList *list, struct Node *node)
{
    struct Node *prev = node->prev;
    struct Node *next = node->next;

    prev->next = next;
    next->prev = prev;
    node->prev = list->header;
    node->next = list->header->next;
    list->header->next->prev = node;
    list->header->next = node;
}

// Add the passed PTE to the back of the passed linked list.
void addBack(struct DLinkedList *list, struct PageTableEntry *page)
{
    struct Node *newNode = malloc(sizeof(struct Node));

    newNode->page = page;

    newNode->next = list->trailer;
    newNode->prev = list->trailer->prev;
    list->trailer->prev->next = newNode;
    list->trailer->prev = newNode;

    list->numNodes++;
}

// Remove the PTE at the back of the linked list.
void rmBack(struct DLinkedList *list)
{
    struct Node *node = list->trailer->prev;

    if (node != list->header) {
        node->prev->next = node->next;
        node->next->prev = node->prev;

        free(node);

        list->numNodes--;
    }
}

// Insert the passed node within the passed linked list to the back of the list.
void insertBack(struct DLinkedList *list, struct Node *node)
{
    struct Node *prev = node->prev;
    struct Node *next = node->next;

    prev->next = next;
    next->prev = prev;
    node->next = list->trailer;
    node->prev = list->trailer->prev;
    list->trailer->prev->next = node;
    list->trailer->prev = node;
}

// Find and return the node containing the passed PTE. If not found, return NULL.
struct Node *findNode(struct DLinkedList *list, struct PageTableEntry *page)
{
    struct Node *currNode = list->header;

    while (currNode->next != list->trailer) {
        currNode = currNode->next;

        if (currNode->page == page) {
            
            return currNode;
        }
    }
    
    return NULL;
}

// Remove the node with the passed PTE from the passed list.
void rmNode(struct DLinkedList *list, struct PageTableEntry *page)
{
    struct Node *node = findNode(list, page);

    if (node != NULL) {
        node->prev->next = node->next;
        node->next->prev = node->prev;

        free(node);

        list->numNodes--;
    }

}

// Used for LRU. Once a page is accessed, move it to the front of the list.
void updateRecency(struct DLinkedList *list, struct PageTableEntry *page)
{
    struct Node *node = findNode(list, page);

    if (node != NULL) {
        insertFront(list, node);
    }
}

// Return the PTE which is next candidate for eviction. If list is empty, returns NULL.
struct PageTableEntry *getLeastRecent(struct DLinkedList *list)
{
    return list->trailer->prev->page;
}

// Free all the nodes within the passed list from memory.
void freeList(struct DLinkedList *list)
{
    while (list->numNodes != 0) {
        rmFront(list);
    }
    
    free(list->header);
    free(list->trailer);
    
}

// Extract the page number from the passed address.
unsigned int getPageNum(unsigned int address)
{
    return address >> 12;
}

// Used for VMS. Find out which process an address corresponds to according to project description.
// See macro PROCESS_B at start of 
unsigned int getProcess(unsigned int address)
{
    return address & 0xF0000000;
}

// Print the passed list for debugging purposes.
void printList(struct DLinkedList *list)
{
    if (list->numNodes == 0) {
        printf("{EMPTY}");
    }

    struct Node *currNode = list->header;

    while (currNode->next != list->trailer) {
        currNode = currNode->next;
        printf("0x%08x ", currNode->page->pageNum);
    }
    printf("\n");
}

// Random replacement policy simulation.
void rdm()
{
    // Seed random function for random replacement.
    srand(time(0));
    struct PageTable pageTable = initPageTable();

    // Some function variables.
    unsigned int address, pageNum, randIndex = 0;
    char rw, exitCh;
    struct PageTableEntry *page;

    // Iterate through trace file reading in address and R / W until end of file.
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        // Keep trace of number of events. Should be 1M at end of execution.
        numEvents++;
        pageNum = getPageNum(address);

        // Print debug info and pause every iteration if requested.
        if (debug) {
            printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);
            printPageTable(pageTable);
            printf("NxtPN: 0x%08x RW: %c \n", pageNum, rw);
            printf("Enter x to exit. ");

            exitCh = getchar();
            if (exitCh == 'X' || exitCh == 'x') {
                free(pageTable.entries);
                exit(0);
            }
        }
        
        // Find the page within in the page table.
        page = findEntry(pageTable, pageNum);

        // If its found, update its dirty bit if its written to.
        if (page != NULL) {
            if (rw == 'W') {
                page->dirty = true;
            }
        }
        // Otherwise, page fault occured.
        else {
            // While page table is not full, fill entry by entry.
            if (!pageTable.isFull) {
                pageTable.entries[pageTable.numEntries].pageNum = pageNum;

                if (rw == 'W') {
                    pageTable.entries[pageTable.numEntries].dirty = true;
                }

                pageTable.numEntries++;
                numReads++;

                // Swap to replacement policy once pageTable is full.
                if (pageTable.numEntries == numFrames) {
                    pageTable.isFull = true;
                }
            }
            // Replace pages at random.
            else {
                randIndex = rand() % numFrames;
                
                if (pageTable.entries[randIndex].dirty) {
                    numWrites++;
                }

                pageTable.entries[randIndex].pageNum = pageNum;
                numReads++;
                
                if (rw == 'R') {
                    pageTable.entries[randIndex].dirty = false;
                }
                else {
                    pageTable.entries[randIndex].dirty = true;
                }
            }
        }
    }

    free(pageTable.entries);
}

// Least recenctly used replacement policy simulation.
void lru()
{
    struct PageTable pageTable = initPageTable();
    struct DLinkedList recencyList = initLinkedList();

    unsigned int address, pageNum;
    char rw, exitCh;

    struct PageTableEntry *page, *pageToRemove;
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        numEvents++;
        pageNum = getPageNum(address);

        if (debug) {
            printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);
            printPageTable(pageTable);
            printf("RECENCY LIST\n");
            printList(&recencyList);
            printf("NxtPN: 0x%08x RW: %c\n", pageNum, rw);
            printf("Enter x to exit. ");

            exitCh = getchar();
            if (exitCh == 'X' || exitCh == 'x') {
                free(pageTable.entries);
                freeList(&recencyList);
                exit(0);
            }
        }
        
        page = findEntry(pageTable, pageNum);

        if (page != NULL) {
            updateRecency(&recencyList, page);

            if (rw == 'W') {
                page->dirty = true;
            }
        }
        else {
            if (!pageTable.isFull) {
                pageTable.entries[pageTable.numEntries].pageNum = pageNum;

                if (rw == 'W') {
                    pageTable.entries[pageTable.numEntries].dirty = true;
                }

                addFront(&recencyList, &pageTable.entries[pageTable.numEntries]);

                pageTable.numEntries++;
                numReads++;

                if (pageTable.numEntries == numFrames) {
                    pageTable.isFull = true;
                }
            }
            else {
                pageToRemove = getLeastRecent(&recencyList);
                
                if (pageToRemove->dirty) {
                    numWrites++;
                }

                pageToRemove->pageNum = pageNum;
                numReads++;
                if (rw == 'W') {
                    pageToRemove->dirty = true;
                }
                else {
                    pageToRemove->dirty = false;
                }

                updateRecency(&recencyList, pageToRemove);
            }
        }
    }

    free(pageTable.entries);
    freeList(&recencyList);
}

void fifo()
{
    struct PageTable pageTable = initPageTable();

    unsigned int address, pageNum, nextPageToRemove = 0;
    char rw, exitCh;

    struct PageTableEntry *page, *pageToRemove;
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        numEvents++;
        pageNum = getPageNum(address);

        if (debug) {
            printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);
            printPageTable(pageTable);
            printf("NxtPN: 0x%08x RW: %c\n", pageNum, rw);
            printf("Enter x to exit. ");

            exitCh = getchar();
            if (exitCh == 'X' || exitCh == 'x') {
                free(pageTable.entries);
                exit(0);
            }
        }

        page = findEntry(pageTable, pageNum);

        // If its found, update its dirty bit if its written to.
        if (page != NULL) {
            if (rw == 'W') {
                page->dirty = true;
            }
        }
        // Otherwise, page fault occured.
        else {
            // While page table is not full, fill entry by entry.
            if (!pageTable.isFull) {
                pageTable.entries[pageTable.numEntries].pageNum = pageNum;

                if (rw == 'W') {
                    pageTable.entries[pageTable.numEntries].dirty = true;
                }

                pageTable.numEntries++;
                numReads++;

                // Swap to replacement policy once pageTable is full.
                if (pageTable.numEntries == numFrames) {
                    pageTable.isFull = true;
                }
            }
            // Replace pages at random.
            else {
                if (pageTable.entries[nextPageToRemove].dirty) {
                    numWrites++;
                }

                pageTable.entries[nextPageToRemove].pageNum = pageNum;
                numReads++;
                
                if (rw == 'R') {
                    pageTable.entries[nextPageToRemove].dirty = false;
                }
                else {
                    pageTable.entries[nextPageToRemove].dirty = true;
                }

                nextPageToRemove++;
                
                if (nextPageToRemove == numFrames) {
                    nextPageToRemove = 0;
                }
            }
        }
    }

    free(pageTable.entries);
}

void vms()
{
    struct PageTable pageTable = initPageTable();
    struct DLinkedList afifo = initLinkedList();
    struct DLinkedList bfifo = initLinkedList();
    struct DLinkedList clean = initLinkedList();
    struct DLinkedList dirty = initLinkedList();

    unsigned int address, pageNum, process, rss = numFrames / 2;
    char rw, exitCh;
    bool isProcessA;

    struct PageTableEntry *page, *pageToRemove;
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        numEvents++;
        pageNum = getPageNum(address);
        process = getProcess(address);
        isProcessA = process != PROCESS_B ? true : false;

        if (debug) {
            printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);
            printPageTable(pageTable);
            printf("A FIFO\n");
            printList(&afifo);
            printf("B FIFO\n");
            printList(&bfifo);
            printf("CLEAN LIST\n");
            printList(&clean);
            printf("DIRTY LIST\n");
            printList(&dirty);
            printf("NxtPN: 0x%08x RW: %c\n", pageNum, rw);
            printf("Enter x to exit. ");

            exitCh = getchar();
            if (exitCh == 'X' || exitCh == 'x') {
                free(pageTable.entries);
                freeList(&afifo);
                freeList(&bfifo);
                freeList(&clean);
                freeList(&dirty);
                exit(0);
            }
        }
        
        page = findEntry(pageTable, pageNum);

        struct Node *node;
        if (page != NULL) {
            node = findNode(&dirty, page);
            if (node != NULL) {
                rmNode(&dirty, page);
            }

            node = findNode(&clean, page);
            if (node != NULL) {
                rmNode(&clean, page);
            }

            if (isProcessA) {
                node = findNode(&afifo, page);

                if (node != NULL) {
                    if (rw == 'W') {
                        node->page->dirty = true;
                    }
                }
                else {
                    addFront(&afifo, page);

                    if (afifo.numNodes >= rss) {
                        pageToRemove = getLeastRecent(&afifo);
                        rmNode(&afifo, pageToRemove);

                        if (pageToRemove->dirty) {
                            addFront(&dirty, pageToRemove);
                        }
                        else {
                            addFront(&clean, pageToRemove);
                        }

                    }
                }
            }
            else {
                node = findNode(&bfifo, page);

                if (node != NULL) {
                    if (rw == 'W') {
                        node->page->dirty = true;
                    }
                }
                else {
                    addFront(&bfifo, page);

                    if (bfifo.numNodes >= rss) {
                        pageToRemove = getLeastRecent(&bfifo);
                        rmNode(&bfifo, pageToRemove);

                        if (pageToRemove->dirty) {
                            addFront(&dirty, pageToRemove);
                        }
                        else {
                            addFront(&clean, pageToRemove);
                        }

                    }
                }
            }
        }
        else {
            if (!pageTable.isFull) {
                pageTable.entries[pageTable.numEntries].pageNum = pageNum;

                if (rw == 'W') {
                    pageTable.entries[pageTable.numEntries].dirty = true;
                }

                if (isProcessA) {
                    if (afifo.numNodes >= rss) {
                        pageToRemove = getLeastRecent(&afifo);
                        rmNode(&afifo, pageToRemove);

                        if (pageToRemove->dirty) {
                            addFront(&dirty, pageToRemove);
                        }
                        else {
                            addFront(&clean, pageToRemove);
                        }
                    }
                    addFront(&afifo, &pageTable.entries[pageTable.numEntries]);
                }
                else {
                    if (bfifo.numNodes >= rss) {
                        pageToRemove = getLeastRecent(&bfifo);
                        rmNode(&bfifo, pageToRemove);

                        if (pageToRemove->dirty) {
                            addFront(&dirty, pageToRemove);
                        }
                        else {
                            addFront(&clean, pageToRemove);
                        }
                    }
                    addFront(&bfifo, &pageTable.entries[pageTable.numEntries]);
                }

                pageTable.numEntries++;
                numReads++;

                if (pageTable.numEntries == numFrames) {
                    pageTable.isFull = true;
                }
            }
            else {
                if (isProcessA) {
                    if (clean.numNodes > 0) {
                        pageToRemove = getLeastRecent(&clean);
                        rmNode(&clean, pageToRemove);
                    } 
                    else if (dirty.numNodes > 0) {
                        pageToRemove = getLeastRecent(&dirty);
                        rmNode(&dirty, pageToRemove); 
                    }
                    else {
                        pageToRemove = getLeastRecent(&afifo);
                        rmNode(&afifo, pageToRemove);
                    }

                    addFront(&afifo, pageToRemove);

                    if (afifo.numNodes > rss) {
                        page = getLeastRecent(&afifo);
                        rmBack(&afifo);

                        if (page->dirty) {
                            addFront(&dirty, page);
                        }
                        else {
                            addFront(&clean, page);
                        }
                    }
                }
                else {
                    if (clean.numNodes > 0) {
                        pageToRemove = getLeastRecent(&clean);
                        rmNode(&clean, pageToRemove);
                    }
                    else if (dirty.numNodes > 0) {
                        pageToRemove = getLeastRecent(&dirty);
                        rmNode(&dirty, pageToRemove);
                    }
                    else {
                        pageToRemove = getLeastRecent(&bfifo);
                        rmNode(&bfifo, pageToRemove);
                    }

                    addFront(&bfifo, pageToRemove);

                    if (bfifo.numNodes > rss) {
                        page = getLeastRecent(&bfifo);
                        rmBack(&bfifo);

                        if (page->dirty) {
                            addFront(&dirty, page);
                        }
                        else {
                            addFront(&clean, page);
                        }
                    }
                }

                if (pageToRemove->dirty) {
                    numWrites++;
                }

                pageToRemove->pageNum = pageNum;
                numReads++;
                if (rw == 'W') {
                    pageToRemove->dirty = true;
                }
                else {
                    pageToRemove->dirty = false;
                }
            }
        }
    }

    free(pageTable.entries);
    freeList(&afifo);
    freeList(&bfifo);
    freeList(&clean);
    freeList(&dirty);
}
