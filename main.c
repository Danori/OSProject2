#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define PAGE_FAULT -1
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
    int index;
    struct Node *prev;
    struct Node *next;
};

struct DLinkedList
{
    int numNodes;
    struct Node *header;
    struct Node *trailer;
};

// Page Table functions.
struct PageTable initPageTable();
void printPageTable(struct PageTable pageTable);
struct PageTableEntry findEntry(struct PageTable pageTable, unsigned int pageNum);

// Linked List functions.
struct DLinkedList initLinkedList();
void addFront(struct DLinkedList *list, int index);
void rmFront(struct DLinkedList *list);
void insertFront(struct DLinkedList *list, struct Node *node);
void addBack(struct DLinkedList *list, int index);
void rmBack(struct DLinkedList *list);
void insertBack(struct DLinkedList *list, struct Node *node);
struct Node *findNode(struct DLinkedList *list, int index);
void rmNode(struct DLinkedList *list, int index);
void updateRecency(struct DLinkedList *list, int index);
int getLeastRecent(struct DLinkedList *list);
void freeList(struct DLinkedList *list);
void printList(struct DLinkedList *list);

// Random helper functions.
unsigned int getPageNum(unsigned int address);
unsigned int getProcess(unsigned int address);

// Functions exclusive to VMS.


// Replacement Policy functions.
void rdm();
void lru();
void fifo();
void vms();

/**
 * Global variables just to avoid passing them / returning them
 * to / from the replacement policy functions.
 */
FILE *traceFile;
int numFrames = 0, numReads = 0, 
numWrites = 0, numEvents = 0;
char *replacementPolicy;
bool debug;

int main(int argc, char *argv[])
{
    // Ensure proper number of arguments.
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

    // Read in remaining arguements.
    sscanf(argv[2], "%d", &numFrames);
    replacementPolicy = argv[3];
    char *debugStr = argv[4];
    if (strcmp(debugStr, "debug") == 0) {
        debug = true;
    }
    else {
        debug = false;
    }

    // Execute respective replacement policy simulation.
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

    // Final output.
    printf("Total memory frames: %d\n", numFrames);
    printf("Events in trace: %d\n", numEvents);
    printf("Total disk reads: %d\n", numReads);
    printf("Total disk writes: %d\n", numWrites);

    return 0;
}

// Create the page table, initialize entries to default values.
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

// Print the page table for debugging purposes.
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

// Find the entry with the pass pageNum. If not found, page fault occured.
unsigned int findEntry(struct PageTable pageTable, unsigned int pageNum)
{
    for (int i = 0; i < numFrames; i++) {
        if (pageTable.entries[i].pageNum == pageNum) {
            return i;
        }
    }

    return PAGE_FAULT;
}

// Create linked list, allocate header / trailer, link them.
struct DLinkedList initLinkedList()
{
    printf("Adding header and trailer nodes ...\n");
    struct DLinkedList list;
    
    list.header = malloc(sizeof(struct Node));
    list.trailer = malloc(sizeof(struct Node));

    list.numNodes = 0;

    list.header->index = -1;
    list.header->next = list.trailer;
    list.header->prev = NULL;

    list.trailer->index = -1;
    list.trailer->prev = list.header;
    list.trailer->next = NULL;

    return list;
}

// Add a node at the head of the linked list.
void addFront(struct DLinkedList *list, int index)
{
    printf("Adding node %d at front ...\n", index);
    struct Node *newNode = malloc(sizeof(struct Node));

    newNode->index = index;

    newNode->prev = list->header;
    newNode->next = list->header->next;
    list->header->next->prev = newNode;
    list->header->next = newNode;

    list->numNodes++;
}

void rmFront(struct DLinkedList *list)
{
    struct Node *node = list->header->next;

    if (node != list->trailer) {
        printf("Removing node %d at front ...\n", node->index);
        node->prev->next = node->next;
        node->next->prev = node->prev;

        free(node);

        list->numNodes--;
    }
}

// Take a node from the list and reinsert it at the head of the list.
void insertFront(struct DLinkedList *list, struct Node *node)
{
    printf("Inserting node %d at front ...\n", node->index);

    struct Node *prev = node->prev;
    struct Node *next = node->next;

    prev->next = next;
    next->prev = prev;
    node->prev = list->header;
    node->next = list->header->next;
    list->header->next->prev = node;
    list->header->next = node;
}

void addBack(struct DLinkedList *list, int index)
{
    printf("Adding node %d at back ...\n", index);
    struct Node *newNode = malloc(sizeof(struct Node));

    newNode->index = index;

    newNode->next = list->trailer;
    newNode->prev = list->trailer->prev;
    list->trailer->prev->next = newNode;
    list->trailer->prev = newNode;

    list->numNodes++;
}

void rmBack(struct DLinkedList *list)
{
    struct Node *node = list->trailer->prev;

    if (node != list->header) {
        printf("Removing node %d at back ...\n", node->index);
        node->prev->next = node->next;
        node->next->prev = node->prev;

        free(node);

        list->numNodes--;
    }
}

// Insert the passed node to the back of the linked list.
void insertBack(struct DLinkedList *list, struct Node *node)
{
    printf("Inserting node %d at back ...\n", node->index);

    struct Node *prev = node->prev;
    struct Node *next = node->next;

    prev->next = next;
    next->prev = prev;
    node->next = list->trailer;
    node->prev = list->trailer->prev;
    list->trailer->prev->next = node;
    list->trailer->prev = node;
}

// Find the node with the passed index in the linked list, return it.
struct Node *findNode(struct DLinkedList *list, int index)
{
    struct Node *currNode = list->header;

    while (currNode->next != list->trailer) {
        currNode = currNode->next;

        if (currNode->index == index) {
            printf("Found node %d ...\n", index);
            return currNode;
        }
    }

    printf("Unable to find node %d ...\n", index);
    return NULL;
}

// Remove a node with the passed index from the linked list.
void rmNode(struct DLinkedList *list, int index)
{
    struct Node *node = findNode(list, index);

    if (node != NULL) {
        printf("Removing node %d ...\n", index);
        node->prev->next = node->next;
        node->next->prev = node->prev;

        free(node);

        list->numNodes--;
    }

}

/**
 * Update the recency of when a page table entry was accessed
 * by moving it to the head of the linked list.
 * The list is organized such that the pages most recently
 * accessed are at the head, while the least recently accessed
 * is at the tail.
 */
void updateRecency(struct DLinkedList *list, int index)
{
    struct Node *node = findNode(list, index);

    if (node != NULL) {
        insertFront(list, node);
    }
}

// Returns the index of the least recently used page table entry.
int getLeastRecent(struct DLinkedList *list)
{
    return list->trailer->prev->index;
}

void freeList(struct DLinkedList *list)
{
    while (list->numNodes != 0) {
        rmFront(list);
    }

    printf("Removing header and trailer sentinel nodes ...\n");
    free(list->header);
    free(list->trailer);
    printf("Entire list successfully freed ...\n");
}

// Extract the page number from the address by performing a 12 bit right-shift.
unsigned int getPageNum(unsigned int address)
{
    return address >> 12;
}

unsigned int getProcess(unsigned int address)
{
    return address & 0xF0000000;
}

// Print the recency list for debugging purposes.
void printList(struct DLinkedList *list)
{
    printf("LINKED LIST\n");

    if (list->numNodes == 0) {
        printf("{EMPTY}");
    }

    struct Node *currNode = list->header;

    while (currNode->next != list->trailer) {
        currNode = currNode->next;
        printf("%d ", currNode->index);
    }
    printf("\n");
}

// Perform simulation with random page replacement.
void rdm()
{
    // Seed random function with current time.
    srand(time(0));

    // Create empty page table.
    struct PageTable pageTable = initPageTable();

    unsigned int address, pageNum;
    char rw, exitCh;

    int index = 0, randIndex = 0;
    // Iterate reading in events until end of file.
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        numEvents++;
        pageNum = getPageNum(address);

        // Print debugging information if requested to.
        if (debug) {
            printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);
            printPageTable(pageTable);
            printf("NxtPN: 0x%08x RW: %c \n", pageNum, rw);
            printf("Enter x to exit. ");

            // Option to terminate early.
            exitCh = getchar();
            if (exitCh == 'X' || exitCh == 'x') {
                free(pageTable.entries);
                exit(0);
            }
        }
        
        // Return index of the PTE with the passed pageNum.
        index = findEntry(pageTable, pageNum);

        // PTE with pageNum is found.
        if (index != PAGE_FAULT) {
            // PTE is simply read, continue to next event.
            if (rw == 'R') {
                continue;
            }
            // Otherwise, set dirty bit, since PTE was written to.
            else {
                pageTable.entries[index].dirty = true;
            }
        }
        // Otherwise, page fault occured.
        else {
            // If the page is not yet full
            if (!pageTable.isFull) {
                // Fill next empty page table entry
                pageTable.entries[pageTable.numEntries].pageNum = pageNum;

                // If written to immediately, set dirty bit accordingly.
                if (rw == 'W') {
                    pageTable.entries[pageTable.numEntries].dirty = true;
                }

                pageTable.numEntries++;
                numReads++;

                // Once page table is full, swap to replacement policy.
                if (pageTable.numEntries == numFrames) {
                    pageTable.isFull = true;
                }
            }
            else {
                // Choose a random page to evict.
                randIndex = rand() % numFrames;
                
                // If page was modified, disk write is required.
                if (pageTable.entries[randIndex].dirty) {
                    numWrites++;
                }

                // Evict current page, read in new one.
                pageTable.entries[randIndex].pageNum = pageNum;
                numReads++;
                // Set dirty bit according to new pages R/W.
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

void lru()
{
    struct PageTable pageTable = initPageTable();
    struct DLinkedList recencyList = initLinkedList();

    unsigned int address, pageNum;
    char rw, exitCh;

    int index = 0, pageToRemoveIndex = 0;
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        numEvents++;
        pageNum = getPageNum(address);

        if (debug) {
            printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);
            printPageTable(pageTable);
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
        
        index = findEntry(pageTable, pageNum);

        if (index != PAGE_FAULT) {
            if (rw == 'R') {
                continue;
            }
            else {
                pageTable.entries[index].dirty = true;
            }

            updateRecency(&recencyList, index);
        }
        else {
            if (!pageTable.isFull) {
                pageTable.entries[pageTable.numEntries].pageNum = pageNum;

                addFront(&recencyList, pageTable.numEntries);

                if (rw == 'W') {
                    pageTable.entries[pageTable.numEntries].dirty = true;
                }

                pageTable.numEntries++;
                numReads++;

                if (pageTable.numEntries == numFrames) {
                    pageTable.isFull = true;
                }
            }
            else {
                pageToRemoveIndex = getLeastRecent(&recencyList);
                
                if (pageTable.entries[pageToRemoveIndex].dirty) {
                    numWrites++;
                }

                pageTable.entries[pageToRemoveIndex].pageNum = pageNum;
                numReads++;
                if (rw == 'R') {
                    pageTable.entries[pageToRemoveIndex].dirty = false;
                }
                else {
                    pageTable.entries[pageToRemoveIndex].dirty = true;
                }

                updateRecency(&recencyList, pageToRemoveIndex);
            }
        }
    }

    free(pageTable.entries);
    freeList(&recencyList);
}

void fifo()
{

}

void vms()
{
    struct PageTable pageTable = initPageTable();
    struct DLinkedList afifo = initLinkedList();
    struct DLinkedList bfifo = initLinkedList();
    struct DLinkedList clean = initLinkedList();
    struct DLinkedList dirty = initLinkedList();

    unsigned int address, pageNum, process;
    char rw, exitCh;

    int index = 0, pageToRemoveIndex = 0, rss = numFrames / 2,
    numAFrames = 0, numBFrames = 0;
    bool isProcessA;
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        numEvents++;
        pageNum = getPageNum(address);
        process = getProcess(address);

        isProcessA = process != PROCESS_B ? true : false;

        if (debug) {
            printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);
            printPageTable(pageTable);
            printList(&afifo);
            printList(&bfifo);
            printList(&clean);
            printList(&dirty);
            printf("NxtPN: 0x%08x RW: %c\n", pageNum, rw);
            printf("Process: 0x%08x\n", process);
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
        
        struct Node *node;
        if (isProcessA) {
            node = findNode(&afifo, pageNum);

            if (node != NULL) {
                continue;
            }
            else {
                addFront(&afifo, pageNum);

                if (afifo.numNodes > rss) {
                    struct Node* nodeToRemove = afifo.trailer->prev;



                    rmBack(&)
                }
            }
        }
        else {
            node = findNode(&bfifo, pageNum);

            if (node != NULL) {
                continue;
            }
            else {

            }
        }
    }

    free(pageTable.entries);
    freeList(&afifo);
    freeList(&bfifo);
    freeList(&clean);
    freeList(&dirty);
}
